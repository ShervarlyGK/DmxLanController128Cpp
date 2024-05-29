#pragma once
//--------------------------------------dmx_lib_section-----------
// *** comment UARTs not used ***
//#define        USE_UART0
//#define        USE_UART1
#define        USE_UART2
#define        USE_UART3

#define RxNotPermanent
#define TxNotPermanent

//include "initializer_list"--------------------------------------
#ifndef __STD_HEADER_INITIALIZER_LIST
#define __STD_HEADER_INITIALIZER_LIST
#pragma GCC visibility push(default)

namespace std { 

template<class T> 
class initializer_list {
  
private:
  const T* array; 
  size_t len; 
  // Initialize from a { ... } construct
  initializer_list(const T *a, size_t l): array(a), len(l) { }
public:
  // default constructor
  initializer_list() : array(NULL), len(0) {}

  size_t size() const { return len; }
  const T *begin() { return array;  }
  const T *end() { return array + len; }
};
}
#endif
//----------------------------------------------------------------

#include "lib_dmx.h"

//----------------------------------------------------------------
#define Tick10ms 2
//#define Btn1pin 30
//#define Btn2pin 31
enum {Btn1pin=30,Btn2pin=31};

extern void btnprocessing();
extern void statescreen();
extern byte wavetstgrgo;  //номер группы для которой запущен WAVE-test....
extern byte wavetsttime;

#ifndef TstGrEnum
#define TstGrEnum
enum TstGrValues {MinVal=0, MaxVal, Tau01s, GrValMaxN}; //GrValMaxN - это количество параметров группы- всегда последняя константа
#endif
extern byte tstvalues[][GrValMaxN];

//----------------------------------------------------------------
#define cBtnLongPr  15 //* 0.1sec
class initializer_list;
class gsBtn
{private:
    enum {ShortPress=1, LongPress};
    byte btn_state=0;
    int btn_time;
    byte btn_pin;    

 public:
    gsBtn (const byte pin) {btn_pin = pin;}
    byte processing()
      { if(digitalRead(btn_pin)==LOW) 
            switch ( btn_state )
                { case 0 : btn_state =1; btn_time=1; break;
                  case 1 : btn_time++; 
                           if(btn_time > cBtnLongPr *10 /Tick10ms) {btn_state =2; return (LongPress);}
                  case 2 : break;
                }
        else switch ( btn_state )
                { case 0 : break;
                  case 1 : btn_state =0; return (ShortPress);
                  case 2 : btn_state =0; 
                }
       return (0);
      }    
};      

class gsBtns
{ private:
    byte btns_n;
    gsBtn **btns;
    byte btn_i, rc;    
    
  public:
  
//    gsBtns (const byte nbtns, const byte btn_pins[])
gsBtns (const byte nbtns, std::initializer_list<byte> btn_pins)
      { btns_n = nbtns;
        btns = new gsBtn* [btns_n];
        for (byte i=0; i < btns_n; i++) 
          { *(btns+i) = new gsBtn( *(btn_pins.begin()+i) );}        
      }
    ~gsBtns ()
      {for (byte i=0; i < btns_n; i++) delete *(btns+i); 
       delete [] btns; btns =nullptr;
      }

    byte processing()
      { rc=0;
        for (btn_i=0; btn_i < btns_n; btn_i++)
        if(rc = ( (*(btns+btn_i))->gsBtn::processing()) ) return (rc);
      }
#define UnDefBtn 0xFF  //Это значение функция возвращает, если ее вызвать до "срабатывания"- (rc !=0)
    byte prsdbtn_i() 
      {if(rc) return (btn_i);
       return( UnDefBtn );
      } 
};  

