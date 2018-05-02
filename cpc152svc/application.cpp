#include "cpc152svc.hpp"
#include <sys/mman.h>
#include <stdio.h>
#include <execinfo.h>
#include <malloc.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <dirent.h>
#include <boost/exception/all.hpp>
#include <linux/watchdog.h>

static boost::mutex mem_locker;
static size_t max_sz      = 0;
static size_t total_alloc = 0;

void * mem_alloc(size_t sz)
{

    //boost::unique_lock<boost::mutex> l(mem_locker);
    max_sz = std::max(max_sz,sz);
    void * ptr = malloc(sz);
    total_alloc+=malloc_usable_size(ptr);
    return ptr;
}

void mem_free(void * ptr)
{
    if(ptr)
    {
        //boost::unique_lock<boost::mutex> l(mem_locker);
        total_alloc-=malloc_usable_size(ptr);
        free(ptr);
    }
}

void * operator new (size_t sz)
{
    return mem_alloc(sz);
}

void * operator new [] (size_t sz)
{
    return mem_alloc(sz);
}

void operator delete(void * ptr)
{
    mem_free(ptr);
}

void operator delete [](void * ptr)
{
    mem_free(ptr);
}

namespace Fastwell
{

app_config tapplication::config;
std::string tapplication::config_file = "cpc152svc.conf";

tapplication * tapplication::app = NULL;
int sin_generate(int16_t * buf, int buf_count, bool is_sin, double ampl, bool positive, bool inverse_negative, int discr = 1, double Freq  = 50);
#ifdef _DEBUG
void test_handle_analog(totd_data * odata);
#endif


const char * tapplication::rdopts_lib = "./libcpc152svc-rdopts.so";
tapplication::tapplication(int argc,char **argv)
{
    app = this;


    server_thread    = NULL;
    poller_thread        = NULL;
    ro_result = -1;


    void * dlib = dlopen(rdopts_lib+2,RTLD_LAZY);
    if(!dlib)
        dlib    = dlopen(rdopts_lib,RTLD_LAZY);
    char * dlerr = nullptr;
    if(dlib)
    {
        dlerror();
        pread_options pro = (pread_options)dlsym(dlib,"read_options");
        dlerr = dlerror();
        if(!dlerr && pro)
            ro_result =   (*pro)(argc,argv,&config);
        dlclose(dlib);
    }

    if(dlerr)
        tapplication::write_syslog(LOG_ALERT,"Shared library error: %s"
                                   "\n",dlerr);
}


int tapplication::run()
{
    int ret = -1;
    int debug_build;
#ifndef _DEBUG
    debug_build = 0;
#else
    debug_build = 1;
#endif
    //if(!geteuid() || debug_build)
    if(debug_build)
    {
      write_syslog(LOG_INFO,"Debug build version\n");
    }

        if(!ro_result && init_application())
        {

            ret = do_run();

        }


//    else
//    {
//        cout<<"Only the user with root access can run this program\nShutdown!\n"<<endl;
//    }

    closelog();
    return ret;


}


int tapplication::init_application()
{
    int log_option = LOG_PID;
    int log_facility = 0;
    if(config.daemon)
    {
        log_facility = LOG_DAEMON;

    }
    else
    {
        log_option   |= LOG_CONS;
        log_facility = LOG_USER;
    }
    openlog(NULL,log_option,log_facility);
    if(config.daemon)
        daemon_start();

    return init_devices();
}

int tapplication::iopl_init  (bool release)
{
#ifdef _DEBUG
    if(!release)
        srand(time(NULL)+getuid());
    return 0;
#else
    return iopl(release ? 0 : 3) ;
#endif
}


int tapplication::open_device(char * dev_name)
{
  return open(dev_name,O_RDWR|O_NONBLOCK);
 //return open(dev_name,O_RDWR);

}

int tapplication::init_dic120(tdic_param * beg, tdic_param * end, int *_count)
{
    int ret = 0;
    int count = 0;

    p55_pga_param pp;
    memset(pp.irqs,1,sizeof(pp.irqs));
    memset(pp.fronts,P55_BOTH_FRONTS,sizeof(pp.fronts));

    while(beg<end)
    {
        int dev_fd = open_device(beg->dev_name);
        if(dev_fd>0)
        {
            ioctl(dev_fd,DIC120_IOC_CLEAR_PGA);
            pp.debounce   = beg->debounce;
            for(int i = 0; i<DIC120_PGA_COUNT; i++)
            {
                if(beg->pga[i])
                {
                    pp.number = i;
                    ioctl(dev_fd,DIC120_IOCW_ADD_PGA,&pp);
                    ++count;

                }
            }
            poller->add_device(dev_fd,beg->scan_freq,true);
            write_syslog(LOG_INFO,"Add discrete device %s\n",beg->dev_name);
        }
        else
            write_syslog(LOG_ALERT,"Error open discrete device %s\n",beg->dev_name);

        ++beg;
    }
    if(_count) *_count = count;
    return ret;
}


int tapplication::init_aic124(taic_param * beg, taic_param * end, int *_count,int * _min_scan_period)
{
    int ret = 0;
    aib_param ap;
    bzero(&ap,sizeof(ap));
    int count = 0,min_scan_period = 1000000;

    ap.count = AIC124_MUX_SINGLE_CHANNELS_COUNT;
    for(int i = 0; i<ap.count; i++) ap.mux_numbers[i] = i;

    while(beg<end)
    {

        int fd = open_device(beg->dev_name);
        if(fd>0)
        {
            //Открыли успешно.
            ioctl(fd,AIC124_IOC_CLEAR_CHANNELS);
            aic124_dacvalues  dv;
            dv.dac_values_count = 1000/(50*beg->scan_period);

         dv.dac_num = 0;
         tdac_param * dp = config.dac_param;
         tdac_param * dpe = dp + 2;
         while(dp<dpe)
          {
            if(dp->freq>0)
            dv.dac_values_count =  sin_generate(dv.dac_values,KERTL_ARRAY_COUNT(dv.dac_values),dp->is_sinus,dp->ampl,!dp->inv_neg,dp->inv_neg,beg->scan_period,dp->freq);
            else
             {
              dv.dac_values_count = 1;
              dv.dac_values[0] = dp->ampl;
             }
            ioctl(fd,AIC124_IOCW_SET_DAC_VALUES,&dv);
            dv.dac_num++;
            ++dp;
          }

            for(int i = 0; i<int(sizeof(beg->channels)/sizeof(beg->channels[0])); i++)
            {
                if(beg->channels[i])
                {
                    ap.aib_number = i;
                    ap.avg        = 0;
                    memset(ap.mux_modes,beg->modes[i] ? 1:0,sizeof(ap.mux_modes));
                    ap.count   = ap.mux_modes[0] ? AIC124_MUX_SINGLE_CHANNELS_COUNT : AIC124_MUX_DIFF_CHANNELS_COUNT;
                    ap.ch_gain = beg->gain;                   
                    min_scan_period = std::min(beg->scan_period,min_scan_period);
                    ioctl(fd,AIC124_IOCW_ADD_AIB,&ap);
                    ++count;

                }
            }
            poller->add_device(fd,beg->scan_period,false);
            write_syslog(LOG_INFO,"Add analog device %s\n",beg->dev_name);
        }
        else
            write_syslog(LOG_ALERT,"Error open analog device %s\n",beg->dev_name);
        ++beg;

    }
    if(_count) *_count = count;
    if(_min_scan_period) *_min_scan_period = std::min(*_min_scan_period,min_scan_period);
    return ret;
}



int tapplication::init_devices()
{

    string storage_path = config.alarms.storage_path;
    if(storage_path.length())
    {

        system::error_code ec;
        filesystem::create_directories(filesystem::path(storage_path),ec);
        if(ec)
            tapplication::write_syslog(LOG_DEBUG,"Create directoryes %s\n%s\n",storage_path.c_str(),ec.message().c_str());
    }

    write_syslog(LOG_INFO,"init devices\n");
    clear_tmp_files();

    totd_data * odata = new totd_data(config.alarms);
    odata->read_alarms();

    data     =  totd_data::shared_ptr            (odata);

    traw_cpc152_server * srv   = new traw_cpc152_server (data,config.srv_cfg.frame_size);
    server =  traw_cpc152_server::shared_ptr            (srv);
    int wdg_dev = 0;
    if(config.watch_dog_enabled)
       {

        wdg_dev = open ("/dev/watchdog",O_RDWR) ;
        write_syslog(LOG_INFO,"Watchdog timer enabled. Open watchdog device %s errno %d\n",wdg_dev>0 ? "success": "ERROR",errno);
        if(wdg_dev>0)
         {
            struct watchdog_info wi;
            if(!ioctl(wdg_dev,WDIOC_GETSUPPORT,&wi))
               {
                write_syslog(LOG_INFO,"Watchdog timer %s ver-%d\n",wi.identity,(int)wi.firmware_version);
               }

         }
       }

    tdev_poller * dp     =  new tdev_poller(data,server,config.devp_qsize,wdg_dev);
    poller   =  boost::shared_ptr<tdev_poller>   (dp);

    //Создаем список устройств для сканировния
    int a_count,d_count,min_scan_period = 100000000;
    tdic_param * dic_param  = config.dev_cfg.dic;
    init_dic120(dic_param, dic_param+config.dev_cfg.dic_count,&d_count);
    taic_param * aic_param  = config.dev_cfg.aic;
    init_aic124(aic_param,aic_param+config.dev_cfg.aic_count,&a_count,&min_scan_period);
    int dev_count = poller->get_device_count();

#ifdef _DEBUG
  // test_handle_analog(odata);
#endif

    if(dev_count && !data->prepare_queues(min_scan_period,a_count,d_count))
           exit(CHILD_NEED_TERMINATE);

    return dev_count;
}

void   tapplication::stop_threads  ()
{

    write_syslog(LOG_INFO,"application stop threads");
    poller->terminate();
    server->get_io_service().stop();

}

int tapplication::do_run()
{
    try
    {

        if(poller->get_device_count())
        {

#ifndef _DEBUG
            if(mlockall(MCL_FUTURE) && tapplication::get_log_level())
                tapplication::write_syslog(LOG_DEBUG,"mlockall fail\n");
#endif
            //sched_param sp;
            //sp.__sched_priority = sched_get_priority_max(SCHED_FIFO);
            //sp.__sched_priority = 20;
            //sched_setscheduler(0, SCHED_FIFO, &sp);
            daemon_work_init();
            iopl_init();

            write_syslog(LOG_INFO,"start application begin\n");          
            write_syslog(LOG_INFO,"create server thread\n");
            server_thread    = new boost::thread (bind(&traw_cpc152_server::start_server,server.get(),config.srv_cfg.server_port   ));

            write_syslog(LOG_INFO,"create POLLER server thread\n");
            poller_thread        = new boost::thread (bind(&tdev_poller::polling,poller.get()));
            write_syslog(LOG_INFO,"Done...\n");

            if(poller_thread       )
               poller_thread       ->join();
            if(server_thread)
               server_thread   ->join();

            delete poller_thread;
            delete server_thread;

            return 0;
        }
        else
        {
            tapplication::write_syslog(LOG_ERR,"No device for polling\n Shutdown...\n");
            return -1;
        }

    }
    catch(boost::exception & bex)
    {

        write_syslog(LOG_EMERG,"Boost exception catched.\n%s\nShutdown...\n",boost::diagnostic_information(bex).c_str());
    }
    catch(...)
    {
        write_syslog(LOG_EMERG,"Exception catched.\nShutdown...\n");
    }

    return -1;
}


/* daemon support*/

const char * tapplication::pid_file_name = "/var/run/cpc152svc.pid";

void tapplication::pid_file( bool create)
{
    if(create)
    {
        FILE* f;
        f = fopen(pid_file_name, "w+");
        if(f)
        {
            fprintf(f, "%u", getpid());
            fclose(f);
        }
    }
    else
        unlink(pid_file_name);
}

int tapplication::daemon_start()
{
    int pid = fork();
    switch(pid)
    {
    case -1:
        write_syslog(LOG_DEBUG,"fork failed. Error start daemon\n");
        exit(-1);
        break;
    case 0:

        close(STDERR_FILENO);
        close(STDIN_FILENO);
        close(STDOUT_FILENO);

        if(setsid() == -1)
        {
            tapplication::write_syslog(LOG_CRIT,"error setsid");
            exit(-1);
        }
        if(chdir("/"))
        {
            tapplication::write_syslog(LOG_CRIT,"error chdir");
            exit(-1);
        }
        //daemon_work_init();
        // return 0;
        daemon_control_proc();
        break;
    default:
        exit(0);//Завершаем запускающий процесс
        break;
    }
    return 0;
}

void tapplication::daemon_control_proc()
{

    int      pid        = -1;
    int      ret_code   =  0;
    int      need_start =  1;

    sigset_t sigset;
    siginfo_t siginfo;

    // настраиваем сигналы которые будем обрабатывать
    sigemptyset(&sigset);

    // сигнал остановки процесса пользователем
    sigaddset(&sigset, SIGQUIT);

    // сигнал для остановки процесса пользователем с терминала
    sigaddset(&sigset, SIGINT);

    // сигнал запроса завершения процесса
    sigaddset(&sigset, SIGTERM);

    // сигнал посылаемый при изменении статуса дочернего процесса
    sigaddset(&sigset, SIGCHLD);

    // пользовательский сигнал который мы будем использовать для обновления конфига
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    // данная функция создаст файл с нашим PID'ом
    pid_file(true);

    // бесконечный цикл работы
    for (;;)
    {
        // если необходимо создать потомка
        if (need_start)
        {
            // создаём потомка
            pid = fork();
        }

        need_start = 1;

        if (pid == -1) // если произошла ошибка
        {
            // запишем в лог сообщение об этом
            tapplication::write_syslog(LOG_INFO,"cpc152svc [MONITOR] Fork failed (%s)\n", strerror(errno));
            exit(-1);
        }
        else if (!pid) // если мы потомок
        {
            // данный код выполняется в потомке
            tapplication::write_syslog(LOG_INFO,"daemon work_init\n");

            void * dlib = dlopen(rdopts_lib+2,RTLD_LAZY);
            if(!dlib)
                dlib    = dlopen(rdopts_lib,RTLD_LAZY);
            if(dlib)
            {
                dlerror();
                pread_config prc = (pread_config) dlsym(dlib,"read_config");
                if(prc)
                {
                    (*prc)("/etc/cpc152svc.conf",&config);
                    dlclose(dlib);

                    tapplication::write_syslog(LOG_INFO,"daemon return to call proc\n");
                    return;
                }
                dlclose(dlib);
            }
            write_syslog(LOG_ALERT,"rdoptions shared library error: %s\n",dlerror());
            exit(CHILD_NEED_TERMINATE);// Что-то пошло не так
        }
        else // если мы родитель
        {
            // данный код выполняется в родителе

            // ожидаем поступление сигнала
            sigwaitinfo(&sigset, &siginfo);

            // если пришел сигнал от потомка
            if (siginfo.si_signo == SIGCHLD)
            {
                // получаем статус завершение
                waitpid(-1,&ret_code,0);


                // преобразуем статус в нормальный вид
                ret_code = WEXITSTATUS(ret_code);

                // если потомок завершил работу с кодом говорящем о том, что нет нужды дальше работать
                if (ret_code == CHILD_NEED_TERMINATE)
                {
                    // запишем в лог сообщени об этом
                    tapplication::write_syslog(LOG_INFO,"cpc152svc [MONITOR] Child stopped\n");

                    // прервем цикл
                    break;
                }
                else if (ret_code == CHILD_NEED_WORK) // если требуется перезапустить потомка
                {
                    // запишем в лог данное событие
                    tapplication::write_syslog(LOG_INFO,"cpc152svc [MONITOR] Child restart\n");
                }
            }
            else if (siginfo.si_signo == SIGUSR1) // если пришел сигнал что необходимо перезагрузить конфиг
            {
                kill(pid, SIGUSR1); // перешлем его потомку
                need_start = 0; // установим флаг что нам не надо запускать потомка заново
            }
            else // если пришел какой-либо другой ожидаемый сигнал
            {
                // запишем в лог информацию о пришедшем сигнале
                tapplication::write_syslog(LOG_INFO,"cpc152svc [MONITOR] Signal %s\n", strsignal(siginfo.si_signo));

                // убьем потомка
                kill(pid, SIGTERM);
                ret_code = 0;
                break;
            }
        }
    }

    // запишем в лог, что мы остановились
    tapplication::write_syslog(LOG_INFO,"cpc152svc [MONITOR] Stop\n");

    // удалим файл с PID'ом
    pid_file(false);

    exit(0);
}

static void signal_error(int sig, siginfo_t *si, void *ptr)
{
    void* ErrorAddr;
    void* Trace[16];
    int    x;
    int    TraceSize;
    char** Messages;

    // запишем в лог что за сигнал пришел
    tapplication::write_syslog(LOG_CRIT,"cpc152svc signal: %s, Addr: 0x%p\n", strsignal(sig), si->si_addr);

   ucontext_t * context = (ucontext_t*)ptr;

#if __WORDSIZE == 64 // если дело имеем с 64 битной ОС
    // получим адрес инструкции которая вызвала ошибку
    ErrorAddr = (void*)context->uc_mcontext.gregs[REG_RIP];
#else
    // получим адрес инструкции которая вызвала ошибку
    ErrorAddr = (void*)context->uc_mcontext.gregs[REG_EIP];
#endif

    // произведем backtrace чтобы получить весь стек вызовов
    TraceSize = backtrace(Trace, 16);
    Trace[1] = ErrorAddr;

    // получим расшифровку трасировки
    Messages = backtrace_symbols(Trace, TraceSize);
    if (Messages)
    {
        tapplication::write_syslog(LOG_CRIT,"== Backtrace ==\n");

        // запишем в лог
        for (x = 1; x < TraceSize; x++)
        {
            tapplication::write_syslog(LOG_CRIT,"%s\n", Messages[x]);
        }

        tapplication::write_syslog(LOG_CRIT,"== End Backtrace ==\n");
        free(Messages);
    }

    tapplication::write_syslog(LOG_INFO,"otd_server stopped\n");

    // остановим все рабочие потоки и корректно закроем всё что надо
    tapplication::app->stop_threads();
    // завершим процесс с кодом требующим перезапуска
    exit(CHILD_NEED_WORK);
}


void tapplication::daemon_work_init()
{



    // сигналы об ошибках в программе будут обрататывать более тщательно
    // указываем что хотим получать расширенную информацию об ошибках
    sigact.sa_flags = SA_SIGINFO;
    // задаем функцию обработчик сигналов
    sigact.sa_sigaction = signal_error;

    sigemptyset(&sigact.sa_mask);

    // установим наш обработчик на сигналы

    sigaction(SIGFPE, &sigact, 0); // ошибка FPU
    sigaction(SIGILL, &sigact, 0); // ошибочная инструкция
    sigaction(SIGSEGV, &sigact, 0); // ошибка доступа к памяти
    sigaction(SIGBUS, &sigact, 0); // ошибка шины, при обращении к физической памяти

    sigemptyset(&sigset);

    // блокируем сигналы которые будем ожидать
    // сигнал остановки процесса пользователем
    sigaddset(&sigset, SIGQUIT);

    // сигнал для остановки процесса пользователем с терминала
    //sigaddset(&sigset, SIGINT);

    // сигнал запроса завершения процесса
    sigaddset(&sigset, SIGTERM);

    // пользовательский сигнал который мы будем использовать для обновления конфига
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    // Установим максимальное кол-во дискрипторов которое можно открыть

    // запишем в лог, что наш демон стартовал
    if(this->config.daemon)
       tapplication::write_syslog(LOG_INFO ,"cpc152svc daemon started\n");

}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"

void tapplication::write_syslog(int pri,const char * fmt,...)
{
    va_list  args;
    va_start(args,fmt);
    char text[1024];
    vsprintf(text,fmt,args);
    if(config.daemon == 0)
        printf(text);
    syslog(pri,text);
}
#pragma GCC diagnostic pop


int sin_generate(int16_t * buf,int buf_count,bool is_sin,double ampl,bool positive
                  ,bool inverse_negative,int discr ,double Freq   )
{
    double PI2     = M_PI*double(2.0);
    double period  = (double(1000)/double(discr))/Freq;
    double delta_angle   = PI2/period;
    double angle = 0;
    double val;
    for(int i = 0; i<buf_count; i++)
    {
        if(is_sin)
            val = sin(angle);
        else
            val = cos(angle);
        if(positive)
            val+=1.0;
        if(inverse_negative)
           val = fabs(val);
        val*=ampl;
        *buf = (int16_t)val;
        ++buf;
        angle+=delta_angle;
        if(angle>PI2) angle-=PI2;
    }
    int ret = (int) floor(period*3.0);
    //printf("dac_count %d",ret);
    return ret;
}



int   arch_filter_bad(const dirent * de)
{
    if(de && de->d_type == DT_REG)
    {
        const char * ptr =  strstr(de->d_name,ALARM_SUFFIX);
        if(ptr)
        {
            ptr = strstr(de->d_name,ALARM_EXTENSION);
            if(ptr)
                return 0;
            else
                return 1;
        }
    }
    return 0;
}

int   tmp_filter(const dirent * de)
{
    if(de && de->d_type == DT_REG)
    {
        const char * ptr =  strstr(de->d_name,".tmp");
        return ptr != NULL ? 1:0;
    }
    return 0;
}




void unlink_files(const char * folder,dirent ** files,int count)
{
    for(int i = 0; i<count; i++)
    {
        string filename = folder;
        dirent *de = files[i] ;
        filename+=de->d_name;
        if(unlink(filename.c_str()))
            tapplication::write_syslog(LOG_ERR,"error delete file %s. errno %d\n",filename.c_str(),errno);

        free(de);
    }
    if(files) free(files);

}

void tapplication::clear_tmp_files()
{
    dirent ** files = NULL;
    int count = scandir(config.alarms.storage_path,&files,arch_filter_bad,alphasort);
    unlink_files(config.alarms.storage_path,files,count);
    //files = NULL;
    //count = scandir("/tmp/",&files,tmp_filter,alphasort);
    //unlink_files(files,count);
}


}
