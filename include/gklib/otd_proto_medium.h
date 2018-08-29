#ifndef _OTD_PROTO_MEDIUM_INC_
#define _OTD_PROTO_MEDIUM_INC_

#ifdef __WIN32__
#include <ke_defs.h>
#include <otd_types.h>
#include <otd_addr.h>
#include <otd_diag.h>
#ifndef RC_INVOKED
#include <math.h>
#endif
#endif
#ifdef __linux
#include <lin_ke_defs.h>
#include <gklib/otd_types.h>
#include <gklib/otd_addr.h>
#include <gklib/otd_diag.h>
#include <math.h>

#endif


#pragma pack(push,1)

#define        MDBR_FIELD_REC_ID      0x00000001
#define        MDBR_FIELD_ID          MDBR_FIELD_REC_ID
#define        MDBR_FIELD_VALUE       0x00000002
#define        MDBR_FIELD_DIAG        0x00000004
#define        MDBR_FIELD_STATE       0x00000008
#define        MDBR_FIELD_TIME        0x00000010

#define        MDBR_FIELD_KVANTS      0x00000020
#define        MDBR_FIELD_MIN_VALUE   0x00000040
#define        MDBR_FIELD_MAX_VALUE   0x00000080

#define        MDBR_FIELD_NAME        0x00000100
#define        MDBR_FIELD_SNAME       0x00000200

#define        MDBR_FIELD_MIN_DANGER  0x00000400
#define        MDBR_FIELD_MAX_DANGER  0x00000800
#define        MDBR_FIELDS_DANGERS    0x00000C00

#define        MDBR_FIELD_MIN_ALARM   0x00001000
#define        MDBR_FIELD_MAX_ALARM   0x00002000
#define        MDBR_FIELDS_ALARMS     0x00003000

#define        MDBR_FIELD_ADDR        0x00004000
#define        MDBR_FIELD_OTDVALUE    0x00008000
#define        MDBR_FIELD_OTDPD       0x00010000
#define        MDBR_FIELD_OPTIONS     0x00020000

#define        MDBR_FIELD_SCALEID     0x00040000
#define        MDBR_FIELD_PERCENT     0x00080000
#define        MDBR_FIELD_LASTVALUE   0x00100000
#define        MDBR_FIELD_SECURITY    0x00200000




#define        MDBR_FIELD_RESERV1     0x00400000
#define        MDBR_FIELD_RESERV2     0x00800000
#define        MDBR_FIELD_RESERV3     0x01000000
#define        MDBR_FIELD_RESERV4     0x02000000
#define        MDBR_FIELD_RESERV5     0x04000000
#define        MDBR_FIELD_RESERV6     0x08000000


#define        MDBR_ALARMS_CHANGED    0x10000000
#define        MDBR_COMBINE_FORMULA   0x40000000 // изменние формулы
#define        MDBR_COMBINE_LIST      0x20000000 // изменение списка

#define        MDBR_RECORD_REMOVED    0x80000000 // Запись удалена

#define        MDBR_VALUE_FIELDS     (MDBR_FIELD_VALUE|MDBR_FIELD_DIAG|MDBR_FIELD_STATE|MDBR_FIELD_TIME|MDBR_FIELD_MIN_VALUE|MDBR_FIELD_MAX_VALUE|MDBR_FIELD_NAME|MDBR_FIELD_SNAME|MDBR_RECORD_REMOVED|MDBR_FIELD_KVANTS|MDBR_FIELD_MIN_DANGER|MDBR_FIELD_MAX_DANGER|MDBR_FIELD_MIN_ALARM|MDBR_FIELD_MAX_ALARM|MDBR_FIELD_OPTIONS)

//Состояние диагностики
#define        MDBR_DIAG_GOOD         0
#define        MDBR_DIAG_PARTIAL      1
#define        MDBR_DIAG_BAD          2


#define        MDBR_OPT_COMBINED      0x80000000  //Признак комбинированного объекта
#define        MDBR_OPT_DANGER        0x40000000  //Аварийный
#define        MDBR_OPT_ALARM         0x20000000  //Предупредительный
#define        MDBR_OPT_CONST_MINMAX  0x10000000  //Заданный минимум, максимум
#define        MDBR_OPT_INVERSE_SCALE 0x08000000  //Инверсная шкала
#define        MDBR_OPT_SIGNED_KVANTS 0x04000000  //Знаковые кванты

#define        MDBR_OPT_FLOAT_TYPE    0x00000001

//Состояния

#define        MDBR_STATE_CONST_MASK  0xFF00
#define        MDBR_STATE_DISCRETE    0x8000  //Дискретный параметр для ТС и ГРУПП
#define        MDBR_STATE_WORKOUT     0x4000  //Выведен из работы
#define        MDBR_STATE_TUFLAG      0x2000  //Наличие ТУ
#define        MDBR_STATE_HIDDEN      0x1000  //Скрытое отображение ТС при нормальном положении
#define        MDBR_STATE_STATIC      0x0800  //Статическая   ТС
#define        MDBR_STATE_NORMAL      0x0400  //Нормальное состояние 0 откл 1 вкл
#define        MDBR_STATE_NORECEIPT   0x0200  //Неквитируемая ТС
#define        MDBR_STATE_PREKVIT     0x0100

#define        MDBR_STATE_DYN_MASK    0x00FF

#define        MDBR_STATE_FLASH       0x0080  //Признак мигания для групповых объектов
#define        MDBR_STATE_ALARM       0x0040  //Признак аварийного состояния
#define        MDBR_STATE_DANGER      0x0020  //Признак предупреждающего состояние

/*резервные состояния*/

#define        MDBR_STATE_DYN_NORMAL  0x0010
#define        MDBR_STATE_DYN_RESERV2 0x0008
#define        MDBR_STATE_DYN_RESERV3 0x0004

#define        MDBR_STATE_TUERROR     0x0002  //Ошибка ТУ
#define        MDBR_STATE_TU          0x0001  //Операця ТУ


struct _MDB_RECORD
{
 DWORD             rec_id;    //Ключ
 float             value;     //Значение посчитанное
 WORD              diag;      //Состояние диагностики
 WORD              state;     //Состояние объекта
 __int64           time;      //Время изменения
 DWORD             kvants;
 float             min_value;
 float             max_value;
 DWORD             name;      //Имя объекта
 DWORD             sname;     //Имя объекта сокращенное

 float             min_danger;
 float             max_danger;
 float             min_alarm;
 float             max_alarm;

 otd_param_addr    addr;       //Адрес параметра
 LONG              otd_val;    //Значение телемеханическое
 DWORD             otd_pd;     //Состояние персональной диагностики

 DWORD             options;
 DWORD             scale_id;  //Id - шкалы
 DWORD             percent;   //Процент нечуствительности * 100

 //Дополнительные поля
 float             last_value; // Последнее значение
 __int64           last_time;  // Время последнее время
 DWORD             security ;  // Группа безопасности
};


typedef _MDB_RECORD   MDB_RECORD;
typedef _MDB_RECORD*  LPMDB_RECORD;
typedef _MDB_RECORD   MDB_SHORT_RECORD;
typedef _MDB_RECORD*  LPMDB_SHORT_RECORD;

struct MDB_LIMITS
{
 DWORD         rec_id;    // ИД записи  которой назначены ограничения
 float         max_danger;// Порог аварийности
 float         min_danger ;

 float         max_alaram;// Порог предупреждения
 float         min_alarm; //
};

typedef MDB_LIMITS* LPMDB_LIMITS;


struct MDB_RECORD_TEXT
{
 DWORD      dw_size;
 DWORD      text_len;
 DWORD      text_size;
 wchar_t    text[ANYSIZE_ARRAY];
};
typedef  MDB_RECORD_TEXT* LPMDB_RECORD_TEXT;

#define MDB_KADR_FIELD_ID                   0x00000001
#define MDB_KADR_FIELD_DIAG                 0x00000002
#define MDB_KADR_FIELD_STATE                0x00000004
#define MDB_KADR_FIELD_CHANGES              0x00000008
#define MDB_KADR_FIELD_DISCRETE_CHANGES     0x00000010
#define MDB_KADR_FIELD_ALARM_CHANGES        0x00000020
#define MDB_KADR_FIELD_ENTRY_COUNT          0x00000040
#define MDB_KADR_FIELD_NAME                 0x00000080
#define MDB_KADR_FIELD_SNAME                0x00000100
#define MDB_KADR_FIELD_IMAGE                0x00000200
#define MDB_KADR_FIELD_FLAGS                0x00000400
#define MDB_KADR_FIELD_SIZE                 0x00000800
#define MDB_KADR_FIELD_TIMES                0x00001000
#define MDB_KADR_FIELD_HIST_BEG_TIME        0x00002000
#define MDB_KADR_FIELD_HIST_END_TIME        0x00004000
#define MDB_KADR_FIELDS_HIST_TIMES          0x00006000

#define MDB_KADR_FIELD_MASK                 0x00007FFF
#define MDB_KADR_FIELD_ENTRYES              0x01000000
#define MDB_KADR_FIELD_VALUES               0x02000000

#define MDB_KADR_REMOVED                    0x80000000

#define MDB_KADR_FLAG_IMAGE_CENTER                0
#define MDB_KADR_FLAG_IMAGE_STRETCH               1
#define MDB_KADR_FLAG_IMAGE_STRETCH_PROPORTIONAL  2
#define MDB_KADR_FLAG_IMAGE_MASK                0x00000003

struct MDB_GPOINT
{
 LONG x,y;
};

typedef  MDB_GPOINT* LPMDB_GPOINT;

struct MDB_GSIZE
{
 LONG cx,cy;
};

typedef  MDB_GSIZE* LPMDB_GSIZE;

#ifdef __WIN32__
inline  tagPOINT & operator << (tagPOINT &pt, MDB_GPOINT & gpt)
{
pt.x = gpt.x;
pt.y = gpt.y;
return pt;
}

inline  tagSIZE & operator << (tagSIZE &sz, MDB_GSIZE & gsz)
{
sz.cx = gsz.cx;
sz.cy = gsz.cy;
return sz;
}

inline  MDB_GPOINT & operator << (MDB_GPOINT &gpt, tagPOINT & pt)
{
gpt.x = pt.x;
gpt.y = pt.y;
return gpt;
}

inline  MDB_GSIZE & operator << (MDB_GSIZE & gsz,  tagSIZE &sz)
{
gsz.cx = sz.cx;
gsz.cy = sz.cy;
return gsz;
}

#endif



struct MDB_KADR
{
 DWORD     kadr_id;      //идентификатор кадра
 WORD      kadr_diag;    //Диагностика   кадра
 WORD      kadr_state;   //Состояние объектов
 WORD      discrete_changes; //Количество изменений ТС
 WORD      alarm_changes;//Количество изменений аварийных и предупредительных
 WORD      kadr_changes; //Количество изменений
 WORD      entry_count;  //Количество объектов в кадре
 DWORD     name_id;      //ID имени кадра
 DWORD     sname_id;     //ID Короткого имени
 DWORD     image_id;     //ID изображения
 DWORD     kadr_flags;   //Флаги кадра
 MDB_GSIZE kadr_size;    //Высота - ширина - кадра
 __int64 image_time;   //Контроль изображения
 __int64 content_time; //Контроль содержания
 __int64 hist_beg_time;    //Время начала аварии
 __int64 hist_end_time;    //Время завершения аварии
};

union kadr_state
{
 WORD kadr_state;
 struct{
        WORD state:8;
        WORD arch_count:4;
        WORD alarm_arch_count:4;
       };
};

typedef MDB_KADR* LPMDB_KADR;

struct  MDB_KADR_ENTRY_OPTIONS
{
 DWORD bkgnd_color;
 DWORD color;
 BYTE  paint_as;     //Вариант отрисовки
 WORD  paint_flags;  //Флаги отрисовки
 union{
       BYTE  normal_value; //Нормальное положение ТС
       struct{
              BYTE  prec  :4;
              BYTE  digits:4;
             };
       };
 BYTE  reserv1;
 BYTE  reserv2;
 BYTE  reserv3;
 BYTE  reserv4;
};

typedef MDB_KADR_ENTRY_OPTIONS* LPMDB_KADR_ENTRY_OPTIONS;

#define TS_PAINT_AS_LAMP             0 //Рисовать лампочкой
#define TS_PAINT_AS_SWITCH_HOR       1 //Рисовать разъединителем горизонтальным
#define TS_PAINT_AS_SWITCH_VERT      2 //Рисовать разъединителем вертикальным
#define TS_PAINT_AS_SHORT_NAME       3 //Рисовать коротким именем

#define TA_PAINT_AS_TEXT             0  //Рисовать текстом
#define TA_PAINT_AS_HBAR             1  //Горизонтальной полосой
#define TA_PAINT_AS_VBAR             2  //Вертикальной полосой
#define TA_PAINT_AS_PIE              3  //Сегментом
#define TA_PAINTAS_DIGITS            4  //Цифрами

#define PAINT_FLAG_SHORT_NAME_RIGHT  1
#define PAINT_FLAG_SHORT_NAME_LEFT   2
#define PAINT_FLAG_SHORT_NAME_TOP    3
#define PAINT_FLAG_SHORT_NAME_BOTTOM 4

#define RDY_PAINT_AS_READY          255 //Рисовать готовность

#define RDY_FLAG_SELECTED           0x8000
#define RDY_FLAG_ALARM_ARCHIVE      0x4000
#define RDY_FLAG_ARCHIVE            0x2000
#define RDY_FLAG_NOTNORMAL          0x0400
#define RDY_FLAG_MANY               0x0200
#define RDY_FLAG_BACKNORMAL         0x0100

#define RDY_FLAG_FLASH              0x0080
#define RDY_FLAG_ALARM              0x0040
#define RDY_FLAG_DANGER             0x0020

#define RDY_FLAG_DANGER_NOTNORMAL   0x0010
#define RDY_FLAG_DANGER_MANY        0x0008
#define RDY_FLAG_DANGER_BACKNORMAL  0x0004
#define RDY_FLAG_TUTR_ERROR         0x0002
#define RDY_FLAG_TUTR               0x0001

#define RDY_FLAG_MASK_FLASHED       (RDY_FLAG_FLASH_STATE|RDY_FLAG_FLASH_NAME)


struct MDB_KADR_ENTRY
{
 DWORD       kadr_id;   // Идентификатор кадра
 DWORD       rec_id;    // Идентификатор записи
 DWORD       number;    // Номер по порядку
 MDB_GPOINT  position;  // Кординаты в кадре
 MDB_GSIZE   size;
 MDB_KADR_ENTRY_OPTIONS options;
};

typedef MDB_KADR_ENTRY* LPMDB_KADR_ENTRY;


struct MDB_KADR_ENTRY_LIST
{
 DWORD dw_size;
 DWORD capacity;
 DWORD count;
 MDB_KADR_ENTRY  entryes[ANYSIZE_ARRAY];
};

typedef MDB_KADR_ENTRY_LIST* LPMDB_KADR_ENTRY_LIST;

/*
  Параметры настройки отрисовки дискретных сигналов
*/

struct MDB_PAINT_VARIANT
{
 DWORD      dw_size;
 DWORD      idx;
 DWORD      name_id;
 MDB_GSIZE  size;
 BOOL       switched;
 DWORD      image_id;
 QWORD      image_time;
 DWORD      mask_id;
 QWORD      mask_time;
};

typedef  MDB_PAINT_VARIANT* LPMDB_PAINT_VARIANT;

#define MDB_PAINT_VARIANT_IDX_MASK      0x00001
#define MDB_PAINT_VARIANT_NAME_MASK     0x00002
#define MDB_PAINT_VARIANT_SIZE_MASK     0x00004
#define MDB_PAINT_VARIANT_SWITCHED_MASK 0x00008
#define MDB_PAINT_VARIANT_IMAGE_MASK    0x00010
#define MDB_PAINT_VARIANT_MASK_MASK     0x00020




#pragma pack(pop)

#ifdef __cplusplus

struct  mdb_record:public _MDB_RECORD
{
  public:
  mdb_record(){FillMemory(this,sizeof(*this),0);addr = sotd_param_addr();rec_id = sname = name = scale_id = -1;diag=MDBR_DIAG_BAD;};
  mdb_record(DWORD _rec_id){FillMemory(this,sizeof(*this),0); this->rec_id =  _rec_id;}
  mdb_record(const MDB_RECORD & rec);
  mdb_record & operator = (const MDB_RECORD & rec);
  mdb_record(otd_param_addr & _addr,DWORD _scale_id=-1,DWORD _opt = 0,DWORD _kv = 250 ,float _scale_end = 250 ,float _scale_begin = 0);
  void  __fastcall get_scale_info(float & scale_begin,float& scale_end,DWORD * kvants = NULL,float * k_value = NULL);
  void  __fastcall set_scale(float  scale_begin,float  scale_end,DWORD _kvants );
  float __fastcall calc_value    (LONG kv );
  DWORD __fastcall calc_kvants   (float val);
  bool  __fastcall is_signed_kvants(){return (options&MDBR_OPT_SIGNED_KVANTS) ? true : false;}
  bool  __fastcall has_max_alarm ()
  {
   return (options&MDBR_OPT_ALARM) && int(1000.0*max_value-1000.0*max_alarm)>0 ? true : false;
  }
  bool  __fastcall has_min_alarm (){return (options&MDBR_OPT_ALARM) && int(1000.0*min_alarm-1000.0*min_value)>0 ? true : false;}
  LONG  __fastcall get_signed_kvants(DWORD kv);

};

typedef mdb_record mdb_short_record;


inline  mdb_record::mdb_record(const MDB_RECORD & rec)
{*this = rec;}

inline  mdb_record & mdb_record::operator = (const MDB_RECORD & rec)
{
 memcpy(this,&rec,sizeof(rec));
 return *this;
}


inline  mdb_record::mdb_record(otd_param_addr & _addr,DWORD _scale_id,DWORD _opt,DWORD _kv  ,float _scale_end  ,float _scale_begin )
{
_scale_id = _scale_id;
rec_id=-1,addr=(_addr),value=last_value=(0),diag=(MDBR_DIAG_BAD),state=(0),otd_val=(-1),otd_pd=(OTD_PDIAG_NODATA);
options=(_opt);
min_danger = max_danger = min_alarm = max_alarm;
set_scale(_scale_begin,_scale_end,_kv);
scale_id=(-1),
percent=(0);
name=(-1),sname=(-1);
time      = last_time = 0;
security = 0;

}

inline void  __fastcall mdb_record::get_scale_info(float & scale_begin,float& scale_end,DWORD * pkvants ,float * pk_value )
{
  scale_begin = min_value;
  scale_end   = max_value;
  if(pkvants)  *pkvants  = kvants;
  if(pk_value) *pk_value = (max_value-min_value)/(float)kvants;
}

inline void  __fastcall mdb_record::set_scale(float  scale_begin,float scale_end,DWORD _kvants )
{
 kvants     = _kvants;
 min_value  = KeRTL::MIN(scale_begin,scale_end);
 max_value  = KeRTL::MAX(scale_begin,scale_end);
 min_alarm  = KeRTL::MAX(min_alarm,min_value  );
 min_danger = KeRTL::MAX(min_alarm,min_danger );
 max_alarm  = KeRTL::MIN(min_alarm,min_value  );
 max_danger = KeRTL::MIN(min_alarm,min_danger );
}

inline LONG __fastcall mdb_record::get_signed_kvants(DWORD kv)
{

union multi_ptr
{
 void    *  v_ptr;
 char    *  c_ptr;
 short   *  s_ptr;
 long    *  l_ptr;
};

 multi_ptr mp;
 mp.v_ptr = &kv;

 LONG result = 0;
 if(kvants<0x80)
    result = (LONG)*mp.c_ptr;
 else
 {
 if(kvants<0x8000)
    result = (LONG)*mp.s_ptr;
    else
    result = (LONG)*mp.l_ptr;
 }
 return result;
}
inline float __fastcall mdb_record::calc_value(LONG kv )
{
 float result;
 bool inv = options&MDBR_OPT_INVERSE_SCALE ? true:false;
 float _min_val = inv ? max_value:min_value;
 float _max_val = inv ? min_value:max_value;
 if(!kvants) return  is_signed_kvants() ? .0 : min_value;
 if(is_signed_kvants())
    {
     result =  ((_max_val)/(float)kvants)*(float)(kv);
    }
 else
   result =  _min_val+((float)kv)*(_max_val-_min_val)/((float)kvants) ;
 return result;
}

inline DWORD __fastcall mdb_record::calc_kvants   (float val)
{
 DWORD ret;
 if(is_signed_kvants())
 {
   float  kv_price = fabs(max_value)/(float)kvants;
   LONG rv = val/kv_price;
   ret  = (DWORD)rv;
 }
 else
 {
  val    -=min_value;
  float  kv_price = (max_value-min_value)/(float)kvants;
  ret = val/kv_price;
  if(this->options&MDBR_OPT_INVERSE_SCALE)
    ret = kvants-ret;
 }
 return ret;
}


//inline float __fastcall mdb_record::calc_value_for(DWORD kv )
//{
//
// if(!kvants) return min_value;
// if(is_signed_kvants())
//    return   (max_value/(float)kvants)*(float)((long)kv);
// return  min_value+((float)kv)*(max_value-min_value)/((float)kvants) ;
//}

#ifdef __linux__
//inline float __fastcall mdb_record::calc_value(LONG kv )
//{

// if(!kvants) return min_value;
// if(is_signed_kvants())
//    return   (max_value/(float)kvants)*(float)((long)kv);
// return  min_value+((float)kv)*(max_value-min_value)/((float)kvants) ;
//}


//inline DWORD __fastcall mdb_record::calc_kvants   (float val)
//{
// val    -=min_value;
// float  kv_price = (max_value-min_value)/(float)kvants;
// DWORD ret = val/kv_price;
// if(this->options&MDBR_OPT_INVERSE_SCALE)
//   ret = kvants-ret;
// return ret;
//}

#endif



struct mdb_record_addr_less
{
 bool operator ()(const MDB_RECORD & rec1,const mdb_record& rec2)
 {  return rec1.addr<rec2.addr;}
};

struct mdb_record_id_less
{
 bool operator ()(const MDB_RECORD & rec1,const MDB_RECORD& rec2)
 { return rec1.rec_id<rec2.rec_id;}

};



#include <functional>
 struct dec_index:public std::unary_function<DWORD,DWORD>
 {
  DWORD index;
  dec_index(DWORD _idx):index(_idx){};
  dec_index(const dec_index & mi):index(mi.index){};
  DWORD operator () (DWORD & x)
  {
     return  x>index ? --x:x;
  }

 };

 struct inc_index:public std::unary_function<DWORD,DWORD>
 {
  DWORD index;
  inc_index(DWORD _idx):index(_idx){};
  inc_index(const inc_index & mi):index(mi.index){};
  DWORD operator () (DWORD & x)
  {
     return  x<index ? x:++x;
  }
 };


  struct mdb_kadr:public MDB_KADR
  {
   mdb_kadr()
   {
    kadr_id = -1;kadr_diag = -1,entry_count = 0,kadr_changes = 0;
    name_id = sname_id = image_id = -1;image_time = content_time = 0;
    kadr_size.cx = 0,kadr_size.cy = 0;kadr_flags = 0;
    discrete_changes = alarm_changes = 0;
    }

   mdb_kadr(DWORD kid,DWORD nid=-1,DWORD iid =-1 )
   {kadr_id = kid,name_id = nid,image_id = iid,kadr_diag = -1,entry_count = 0;
   image_time = content_time = 0;kadr_size.cx = 0,kadr_size.cy = 0;
   kadr_flags = 0;sname_id = -1;
   discrete_changes = alarm_changes = 0;
   }

   mdb_kadr& operator = (const MDB_KADR & kdr)
   {
     memcpy(this,&kdr,sizeof(kdr));
     return * this;
   }

   mdb_kadr(const  MDB_KADR & kdr){*this = kdr;}
  };

  inline bool operator < (const mdb_kadr & k1,const mdb_kadr & k2)
  {
   return k1.kadr_id<k2.kadr_id ? true:false;
  }

  struct mdb_kadr_entry:public MDB_KADR_ENTRY
  {
    mdb_kadr_entry(){kadr_id = rec_id = number = -1;position.x = position.y = 0;size.cx = size.cy = 0;ZeroMemory(&options,sizeof(options)); }
    mdb_kadr_entry(DWORD kid,DWORD rid,DWORD _number )
    {kadr_id = kid,rec_id = rid,number = _number,position.x = position.y = 0;size.cx = size.cy = 0;ZeroMemory(&options,sizeof(options));}
    mdb_kadr_entry(const MDB_KADR_ENTRY & ke){*this = ke;}
    mdb_kadr_entry & operator = (const MDB_KADR_ENTRY & ke)
    { kadr_id = ke.kadr_id,rec_id = ke.rec_id,number = ke.number,position = ke.position,size = ke.size; options = ke.options;return *this ;}
  };


 inline bool operator < ( const MDB_KADR_ENTRY & ke1, const MDB_KADR_ENTRY & ke2)
  {
   if(ke1.kadr_id== ke2.kadr_id)
    {
     if(ke1.rec_id == ke2.rec_id)
        return ke1.number < ke2.number ? true:false;
     return ke1.rec_id < ke2.rec_id ? true:false;
    }
    return ke1.kadr_id < ke2.kadr_id ? true:false;
  }

  inline bool operator == ( const MDB_KADR_ENTRY & ke1, const MDB_KADR_ENTRY & ke2)
  {
   return (ke1.kadr_id == ke2.kadr_id && ke1.rec_id == ke2.rec_id && ke1.number == ke2.number) ? true:false;
  }

  inline bool operator != ( const MDB_KADR_ENTRY & ke1, const MDB_KADR_ENTRY & ke2)
  { return !(ke1==ke2) ;}


  struct mdb_paint_variant:public MDB_PAINT_VARIANT
  {
    void __fastcall init(DWORD idx,DWORD name_id = -1,BOOL switched = 0,DWORD image_id = -1,QWORD  image_time = -1,DWORD  mask_id = 1,QWORD  mask_time = -1,MDB_GSIZE * size = NULL);
    mdb_paint_variant(){init(-1);}
    mdb_paint_variant(const mdb_paint_variant & src){*this = src;}
    mdb_paint_variant(const MDB_PAINT_VARIANT * src){memcpy(this,src,sizeof(*this));}
    mdb_paint_variant & operator =(const mdb_paint_variant & src){memcpy(this,&src,sizeof(*this)); return *this;}

    struct comparer
    {
     int operator ()(const mdb_paint_variant & pv1,const mdb_paint_variant & pv2);
    };

    bool operator < (const mdb_paint_variant & pv) {return comparer()(*this,pv) < 0 ? true:false ;}
  };

 inline   void __fastcall mdb_paint_variant::init(DWORD      _idx ,DWORD  _name_id ,BOOL _switched ,DWORD _image_id ,QWORD  _image_time ,DWORD  _mask_id ,QWORD  _mask_time ,MDB_GSIZE * _size )
 {
  dw_size = sizeof(MDB_PAINT_VARIANT);
  idx        = _idx;
  name_id    = _name_id      ;
  switched   = _switched     ;
  image_id   = _image_id     ;
  image_time = _image_time ;
  mask_id    = _mask_id    ;
  mask_time  = _mask_time  ;
 if(_size)
  {
   size.cx =  _size->cx;
   size.cy =  _size->cy;
  }
  else
  size.cx = size.cy = 0;
 }

inline  int mdb_paint_variant::comparer::operator ()(const mdb_paint_variant & pv1,const mdb_paint_variant & pv2)
     {
       int ret = 0;
       if(pv1.idx != pv2.idx)
       {
        if (pv1.idx < pv2.idx) ret = -int( pv2.idx - pv1.idx) ;
        else
          ret = int( pv1.idx - pv2.idx);
       }
      return ret;
     }
inline bool operator == (const mdb_paint_variant & pv1,const mdb_paint_variant & pv2)
{
  return mdb_paint_variant::comparer()(pv1,pv2) == 0 ? true : false;
}



#endif // __cplusplus

#endif
