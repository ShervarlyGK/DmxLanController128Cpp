//=======================================================================================================================
#include "diol1.h"
#include <EEPROM.h>
#include <Ethernet3.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>
#include <stdlib.h>
LiquidCrystal_I2C lcd(0x3F,16,2);
byte mac[]={0xDE,0xAD,0xBE,0x71,0x72,0x61};
byte ipaddr[]={192,168,1,11};
EthernetUDP Udp;

CArduinoDmx *ODmxp = &ArduinoDmx2;
CArduinoDmx *IDmxp = &ArduinoDmx3;

#define DmxOChLast 64  
         // не более 239 каналов
#define DmxIChLast 40 

#define indCharL 50
#define chGrarL 10
#define PrecShift 9

#define InpChNum  40
#define DestChArL  80

#define UdpPacRecL 508
#define UdpPacMaxL 1024
#define UdpPacMaxL2 250    
                    //максимальная длина добавочного скрипта
#define indx_t byte

unsigned long timems;
byte procerror=0;
byte busyflag=0;            //устанавливается в processtep() и в scriptstep() если выполняется операция
byte scrresident=0;         //устанавливается, если скрипт резидентный
byte indealchs[indCharL];
struct chgr_t {int stepsleft; long longlevel; long longstep; byte grtype;} chgrps [chGrarL];
struct sktxt_t {unsigned int scrtm; unsigned long scrtms0;
                int lbind = -1; byte lpcnt =0; byte inloop =0;    //управляющие циклом переменные 
               } scrktxreg, scrktxadd;

byte sourcharr[InpChNum];
byte destcharr[DestChArL];

int scripti= -1, scriptl=0;
int scripti2= -1, scriptl2=0;

//unsigned int scripttm;
//unsigned long scripttms0;
byte scriptp[UdpPacMaxL];
byte scriptp2[UdpPacMaxL2];  

//int labelind = -1; byte loopcount =0, inloop =0;    //управляющие циклом переменные 

int bufbusyl=0;
byte udpbuff[UdpPacMaxL];

unsigned packnum=0;

byte memory[DmxOChLast];

//============================Processing====================================================
enum grTypes {Empty=0, DmxLED, DmxFilament, GsFilament};

void grclear(indx_t i)
  { chgrps[i].stepsleft=0;  chgrps[i].grtype=Empty; chgrps[i].longlevel = chgrps[i].longstep =0;
  }

void procesreset()
  { for(indx_t i=0; i < indCharL; i++) indealchs[i]=0;
    for(indx_t i=0; i < chGrarL; i++) grclear(i);
  }


void dmxlgrstep(indx_t *ch_ip, indx_t gr_i ) 
  { long x;
    uint8_t value, channl;
    indx_t ch_i1;
    
    ch_i1 = *ch_ip -1;
    x = (chgrps[gr_i].longlevel += chgrps[gr_i].longstep);    if( x<0 ) x=0;
    value = x >> PrecShift; 
    channl =indealchs[*ch_ip];
    do
      { *(ODmxp->TxBuffer + channl -1)= value;  //загрузка нового значения канала в буфер dmx
        channl = indealchs[++(*ch_ip)]; 
      }
    while( channl );                      //обрабатываем все каналы
    if( --(chgrps[gr_i].stepsleft) ==0 )  //проверка остатка шагов и удаление инф.о группе, если шагов не осталось
      {
       for(indx_t i=ch_i1; i < *ch_ip; i++) indealchs[i]=0;
       grclear(gr_i)  ;
      }
  }

void processtep()
  { indx_t gr_i, ch_i=0;
    do {    
        for (; ch_i <indCharL; ch_i++) if( indealchs[ch_i] ) break; //пропускаем нули
        if( ch_i >= indCharL )  return;   //групп не найдено
        
        gr_i = indealchs[ch_i++] -1;    //индекс группы смещен на -1 от номера группы
        if( ! indealchs[ch_i] ) { procerror |= 2; continue;} //!!! нет списка каналов  ====2=!!!!!
        if( chgrps[gr_i].stepsleft==0 )
                                { procerror |= 4; continue;} //!!! остаток шагов группы равен нулю  ====4=!!!!!
        busyflag=1;         //---установка признака занятости------------                                
        switch ( chgrps[gr_i].grtype )
          { case DmxLED:
              dmxlgrstep( &ch_i, gr_i ); //ch_i указывает на номер первого канала группы в массиве indealchs[]
              break;
            case Empty: procerror |= 1; //!!!тип группы "0"                     ====1=!!!!!
            case DmxFilament:
            case GsFilament:
            default:
              for (; ch_i <indCharL; ch_i++) if( ! indealchs[ch_i] ) break; //пропускаем ненули
              break;
          } 
       } while (1);
  }

indx_t dmxlgrinit(byte firstch, byte chnum, byte value, int dt)  // dt/100=seconds; chnum-колич каналов в группе
  { byte oldvalue, flag;
    indx_t gr_i, ch_i, i1=0; 
    int stepnum;
    if(chnum==0 || firstch==0) { procerror |= 1; return(-1); }  //!!! недопустимые нулевые параметры ====1=!!!!!
    if(firstch > DmxOChLast)   { procerror |= 8; return(-1); }  //!!! недопустимый номер канала  ====8=!!!!!
    if(dt > 1020) dt=1020;
//-----ищем пустую группу
    for( gr_i=0; gr_i < chGrarL; gr_i++)  if( chgrps[gr_i].grtype==Empty) break;
    if( gr_i >= chGrarL) {procerror |= 16; return(-1); };        //!!! все группы заняты ====16=!!!!!!!!
    
    chgrps[gr_i].grtype = DmxLED;
    chgrps[gr_i].stepsleft = stepnum = dt ? dt/Tick10ms : 1;
    chgrps[gr_i].longlevel = long( oldvalue = *(ODmxp->TxBuffer + firstch -1) ) << PrecShift;
//    memory[firstch -1] = oldvalue;            //запоминаем бывшее значение канала в памяти
    if( value >= oldvalue )
           chgrps[gr_i].longstep =  ( long(value - oldvalue) << PrecShift ) / stepnum;
      else chgrps[gr_i].longstep = -( long(oldvalue - value) << PrecShift ) / stepnum;
//-----ищем место в indealchs...
    for ( ch_i=0; ch_i <indCharL-2-chnum ; ch_i++)
        { if( indealchs[ch_i] ) {i1 = ch_i +1; continue;} //пропускаем ненули, i1 указывает на символ после ненуля
            else if( ch_i >= i1 +chnum +2 ) 
                    { indealchs[++i1] = gr_i +1;  //номер группы сдвинут на +1, чтобы не было нулевой группы
                      indealchs[++i1] = firstch;  return(++i1); // возвращаем индекс ячейки второго канала в группе
                    }
        }
    procerror |= 32;               return(-1);      //!!! не найдено место в indealchs[]  ====32=!!!!!
  }
  

void dmxlchadd(indx_t ch_i, byte chanel)
  { byte prevch;
    if(ch_i==0 || chanel==0) { procerror |= 1; return; }           //!!! недопустимые нулевые параметры ====1=!!!!
    if(chanel > DmxOChLast)   { procerror |= 8; return; }  //!!! недопустимый номер канала ====8=!!!!
    prevch = indealchs[ch_i -1];
    if(prevch==0 || indealchs[ch_i] !=0 || indealchs[ch_i +1] !=0) 
                             { procerror |= 64; return; };          //!!! неверные значения в indealchs[]  ====64=!!!!!!
                             
//    *(memory + chanel -1) =  *(ODmxp->TxBuffer + chanel -1);       //запоминаем бывшее значение канала в памяти               
    *(ODmxp->TxBuffer + chanel -1)= *(ODmxp->TxBuffer + prevch -1);  //начальные значения каналов выравниваем по первому в группе 
    indealchs[ch_i] = chanel;
  }

//===============================Projection=================================================

void projreset()
  { for(byte i=0; i < InpChNum; i++) sourcharr[i]=0;
  }

  
void projection()
  { byte i1, i2, value;   //отображение входных (source) каналов начинается с первого канала
                          //списки каналов назначения находятся в массиве destcharr[];
                          //sourcharr[] содержит границы этих списков
    i1=0;
    for( byte i=0; i < InpChNum; i++)
      { i2=sourcharr[i];
        value= *(IDmxp->RxBuffer + i);
        while(i1 < i2)  *(ODmxp->TxBuffer + destcharr[i1++]) = value;
      }
  }

void addprojnew(byte inpch, byte destch)    //inpchan = 1...4...
  { byte idx;
    inpch--;  destch--;   //смещение канала в буфере 0...3...
    if(inpch >= InpChNum)   { procerror |= 128 + 1; return; };   //недопустимый номер канала источника или назначения ===129=!!!!!
    if(destch > DmxOChLast) { procerror |= 128 + 1; return; };   //недопустимый номер канала источника или назначения ===129=!!!!!
    
    if(inpch >0) idx = sourcharr[inpch -1];
        else     idx =0;
    if(idx >= DestChArL)  { procerror |= 128 + 2; return; };   //массив назначений переполнен ===130=!!!!!
    
    for(byte i=inpch; i<InpChNum; i++) if(sourcharr[i] != idx) procerror |= 128 + 4;
                                          //ошибка применения функции или структуры sourcharr[]  ===132=!!!!!
    destcharr[idx] = destch;
    for(byte i=inpch; i<InpChNum; i++)  sourcharr[i]=idx +1; 
  }

void addprojnext(byte inpch, byte destch)   //inpchan = 1...4...
  { byte idx;
    inpch--;  destch--;   //смещение канала в буфере 0...3...
    
    if(inpch >= InpChNum) { procerror |= 128 + 1; return; };   //недопустимый номер канала источника ===129=!!!!!
    
    idx = sourcharr[inpch];
    if(idx >= DestChArL)  { procerror |= 128 + 2; return; };   //массив назначений переполнен ===130=!!!!!
    
    for(byte i=inpch+1; i<InpChNum; i++) if(sourcharr[i] != idx) procerror |= 128 + 4;
                                          //ошибка применения функции или структуры sourcharr[]  ===132=!!!!!
    destcharr[idx] = destch;
    sourcharr[inpch] = ++idx;
    for(byte i=inpch+1; i<InpChNum; i++)  sourcharr[i]=idx; 
  }

byte projlastsrch()
  { byte previdx= sourcharr[InpChNum-1];
    if( previdx ==0 ) return 0;
    for(byte i=InpChNum-2; i >=0; i--) 
      { if( sourcharr[i] > previdx) procerror |= 128 + 4;  //ошибка структуры sourcharr[]  ===132=!!!!!
        if( sourcharr[i] == previdx ) continue;
        return i+2;       
      }
    return 1;
  }
  
//==================================Scripts=================================================================
//int scripti= -1, scriptl=0;
//unsigned int scripttm;
//unsigned long scripttms0;
//byte scriptp[UdpPacMaxL];              
//char frame[]="!@250 #2/s50 s1,2/ @100 L #2/0,0 @100 #3/250,250 @250 #3/0,250 r3 @250 #4/250,250 @250 #4/0,250 @500 #2,4/220,500 @500 #2,4/0,400 @500 #2,3,4/0,0 $";               
//int labelind = -1; byte loopcount =0, inloop =0;    //управляющие циклом переменные 

enum ScriptCmd { Htetr =0xF0, Proces, ProcVal, Delay, BackVal, StoreVal, ProjSet, ProjRes, Label, Repeat, CmdEnd };  //BackVal-берем значение из memory +++Ver10++++++++++++++++

 void scriptstep(byte * scriptp, int& scripti, int scriptl, byte scrresident, struct sktxt_t* kontext )
  { byte ch, ch_i, value, istep; int ch1i;  indx_t nxchi; unsigned int dt;
    
    if( scripti <0 ) return;
    if(scripti ==0)           // инициализация обработки скрипта
      { kontext->scrtm=0; kontext->scrtms0 = millis();
        kontext->inloop =0; kontext->lbind =-1;   }
    busyflag=1;       
    if( millis()-kontext->scrtms0 < (unsigned long) kontext->scrtm *10 ) return; //время не настало

    while(scripti < scriptl)
      {switch( *(scriptp + scripti))
        { case Proces:
          
            ch1i= ++scripti;           //указывает на 1-ый номер канала
            while( ((ch = *(scriptp+scripti)) & Htetr) != Htetr)          //пропускаем номера каналов
                    {scripti++; if(scripti >= scriptl) {procerror |= 128 + 8; scripti =-1; return; }} //====136=!!!!!!!!
                                                  //нарушена структура скрипта
            if(scripti == ch1i) {procerror |= 128 + 16; scripti =-1; return; }                        //====144=!!!!!!!!
            scripti++;    //указывает на байт после разделителя
            switch( ch )
              {case ProcVal:
                  value = *(scriptp + scripti); istep=3;
                  dt = ( *(scriptp + scripti+1) <<8) + *(scriptp + scripti+2); break;
               case BackVal:                                                              // from memory
                  value = memory[ *(scriptp + ch1i) -1 ]; istep=2;
                  dt = ( *(scriptp + scripti) <<8) + *(scriptp + scripti+1); break;
               default: procerror |= 128 + 8; scripti =-1; return;  //номер канала отсутствует или больше 239 ====136=!!!!!!!!
              }
            
            nxchi = dmxlgrinit( *(scriptp + ch1i), scripti -ch1i -1, value, dt);
            for(ch1i++; ch1i < scripti-1; ch1i++)   dmxlchadd( nxchi++, *(scriptp + ch1i));
            scripti += istep;                                   
            break;            
          case Delay:
            dt = ( *(scriptp + scripti+1) <<8) + *(scriptp + scripti+2);
            if( kontext->scrtm > 0xFFFF - dt ) {procerror |= 128 + 8 +2; scripti =-1; return; }   //слишком долгий скрипт ===138=!!!!!
            kontext->scrtm += dt;
            scripti +=3;  return;
            
          case StoreVal:         // запоминаем значения каналов в memory
            scripti++;           //указывает на 1-ый номер канала
            while( ((ch = *(scriptp+scripti)) & Htetr) != Htetr)          //перебираем номера каналов
                    {memory[ch-1] = *(ODmxp->TxBuffer + ch -1); scripti++; 
                     if(scripti >= scriptl) {procerror |= 128 + 8; scripti =-1; return; }       //====136=!!!!!!!!
                    }
            if( ch != ProcVal) {procerror |= 128 + 8; scripti =-1; return; }                    //====136=!!!!!!!!
            scripti++;  break;
            
          case ProjRes:
            projreset();  
            scripti++;   break;
            
          case ProjSet:
            scripti++; ch_i = *(scriptp + scripti++);
            if( *(scriptp + scripti++) != ProcVal) {procerror |= 128 + 8; scripti =-1; return; }   //====136=!!!!!!!!
            ch = *(scriptp + scripti++);  if( (ch & Htetr) == Htetr) {procerror |= 128 + 8; scripti =-1; return; }   //====136=!!!!!!!!
            addprojnew(ch_i, ch);
            while( ((ch = *(scriptp+scripti)) & Htetr) != Htetr)   
              { addprojnext(ch_i, ch);  scripti++;
              }
            if( ch != CmdEnd ) {procerror |= 128 + 8; scripti =-1; return; }                      //====136=!!!!!!!!
            scripti++; break;
            
          case Label:
            kontext->lbind = scripti++;  break;
            
          case Repeat:            // проверку на невложенность циклов СДЕЛАТЬ В Convert !!!!!!!!
            if( kontext->lbind < 0 )  {procerror |= 128 + 8 +1; scripti =-1; return; }  // ошибка организации цикла ==137=!!!!!!!!
            if( ! kontext->inloop ) 
              { kontext->inloop =1; scripti++; kontext->lpcnt = *(scriptp+scripti);
                if( ((kontext->lpcnt & Htetr) == Htetr) || (kontext->lpcnt ==0) )  {procerror |= 128 + 8 +1; scripti =-1; return; }  //==137=!!!!!!!!
              }
             else
                if( --(kontext->lpcnt) ==0 ) { kontext->inloop =0; kontext->lbind =-1; scripti +=2; break; };  //нормальный выход из цикла
              
            scripti = kontext->lbind;  break;     //выполняем переход на начало тела цикла
              
          default: procerror |= 128 + 8; scripti =-1; return;  //нарушена структура скрипта        ====136=!!!!!!!!
        }
    } 
  //---------------------------завершение или зацикливание, если скрипт резидентный
  if(scrresident) {scripti = 0;  return;} 
  scripti = -1;  return;  //нормальное завершение скрипта
  }                      
//   Htetr =0xF0, Proces, ProcVal, Delay, BackVal, StoreVal, ProjSet, ProjRes, Label, Repeat, CmdEnd  - управляющие коды скрипта

//=============================================================
void scriptcancel()
  {scripti = -1; scripti2 = -1;
   scrresident=0;
  }

//==============================================CONVERTion==================================================  
byte convert1(byte * udpbuf, byte * scriptp, int& scriptl)
  { byte * scriptp0; byte * bufendp;
    union int2by {int tm; int vlw;
                  byte by[2];
                  byte vl;     }val;
    byte ch_i, ch_o;
    
    bufendp = udpbuf + bufbusyl; scriptp0 = scriptp;
    if( (*(udpbuf++)) & 0xC0 ) {procerror |= 128 + 32; return(0); } //неверный первый байт пакета или ошибка структуры пакета =160======

    while( udpbuf < bufendp )
      switch( *(udpbuf++))
        {
          case '#':
              *(scriptp++) = Proces;
              do {val.vl = atoi((char*)udpbuf); if( (val.vl >239) || val.vl ==0) {procerror |= 128+16; return(0);}
                                                                        //номер канала отсутствует или больше 239 =144=======
                  *(scriptp++) = val.vl; 
                  while( isdigit( *udpbuf)) udpbuf++ ;
                 } while( *(udpbuf++)!='/');
              if( *udpbuf != 's')
                { val.vl = atoi((char*)udpbuf);   while( isdigit( *udpbuf)) udpbuf++ ;    //обработка '/'
                  *(scriptp++) =ProcVal;  *(scriptp++) = val.vl;
                }
               else { udpbuf++; *(scriptp++) =BackVal; }                    //обработка '/s'
               
              val.tm = atoi((char*)++udpbuf); while( isdigit( *udpbuf)) udpbuf++ ; 
               *(scriptp++) = val.by[1];  *(scriptp++) = val.by[0];   break;
              
          case '@':
              *(scriptp++) = Delay;
              val.tm = atoi((char*)udpbuf); while( isdigit( *udpbuf)) udpbuf++ ;
                *(scriptp++) = val.by[1];  *(scriptp++) = val.by[0];  break;
                
          case '$': scriptl =scriptp-scriptp0; return(1);   //успешное завершение, установка длины скрипта scriptl

          case 's':  
              *(scriptp++) = StoreVal; 
              do { val.vl = atoi((char*)udpbuf); if( (val.vl >239) || val.vl ==0) {procerror |= 128+16; return(0);}
                                                                       //номер канала отсутствует или больше 239 ===144====
                   *(scriptp++) = val.vl;  while( isdigit( *udpbuf)) udpbuf++ ;
                 } while( *(udpbuf++)!='/');
              *(scriptp++) =ProcVal;  break;
              
          case 'L': 
              *(scriptp++) = Label;  break;
              
          case 'r':
              if( val.vlw = atoi((char*)udpbuf) )
                if( val.vlw <= 255 ) 
                  { *(scriptp++) = Repeat;  *(scriptp++) = val.vl; }
              while( isdigit( *udpbuf)) udpbuf++ ;  break; 
                 
          case '*': *(scriptp++) = ProjRes;  break;
//--------------------------------------------------------------------- здесь и в других местах доделать контроль диапазона значений с исп. union
          case '+': ch_i = atoi((char*)udpbuf); while( isdigit( *udpbuf)) udpbuf++ ;
              if( *udpbuf != '/') { procerror |= 128+64+8; return(0); };  //ошибка при разборе пакета   ===200=======
              ch_o = atoi((char*)++udpbuf);  while( isdigit( *udpbuf)) udpbuf++;
              *(scriptp++) = ProjSet;  *(scriptp++) = ch_i;  *(scriptp++) = ProcVal;  *(scriptp++) = ch_o;
              while ( *udpbuf == ',') 
                { ch_o = atoi((char*)++udpbuf);  while( isdigit( *udpbuf)) udpbuf++;  *(scriptp++) = ch_o; };
              *(scriptp++) = CmdEnd;  break;

          case ' ': break;          
          case 0x0A: break;  
          case 0x0D: break;         //допустимы пробелы, ВК, ПС между тегами скрипта
          
          default : procerror |= 128+32; return(0);       //неверный тип пакета или ошибка структуры пакета  =160======
        }
    procerror |= 128+32; return(0);  //                                                           ====160======
  }
/*
void convert2(byte * udpbuf)
  { byte ch_i, ch_o;
  
    if( *(udpbuf++) != '=') {procerror |= 128 + 32; return; } //неверный тип пакета или ошибка структуры пакета
    while(1)
      switch( *(udpbuf++))
        {
          case '+': ch_i = atoi((char*)udpbuf); while(isdigit( *(++udpbuf)));
              if( *udpbuf != '/') { procerror |= 128+64+8; return; };  //ошибка при разборе пакета
              ch_o = atoi((char*)++udpbuf);  while(isdigit( *(++udpbuf)));
              addprojnew(ch_i, ch_o);
              while ( *udpbuf == ',') 
                { ch_o = atoi((char*)++udpbuf);  while(isdigit( *(++udpbuf))); 
                  addprojnext(ch_i, ch_o);
                };
              break;

          case '*': projreset();
              break;
          case '$': return;                          //успешное завершение
          case ' ': break;                              //допустимы пробелы между тегами скрипта
          default : procerror |= 128+32; return;   //неверный тип пакета или ошибка структуры пакета =160=!!!!!!!!
        }  
  }
 */ 
//=================================================GETPACKET==============================================  

// int bufbusyl=0;
// byte udpbuff[UdpPacMaxL];

//-------------------------очистка буфера Ethernet контроллера  
void udpclear()
  { byte errbuf[16];
    while( Udp.available() ) 
         { Udp.read(errbuf, 16); 
           procerror |= 128+64;   //аварийная очистка буфера - либо ошибка "1" либо ?               =192=!!!!!!!!!!!!!!!!!
         }
  }
  
void getpack()
  {    
   switch ( udpbuff[0] )
//экстренный скрипт
    { case '!' :  if( udpbuff[bufbusyl-1] != '$')  
                    { procerror |= 128+64+2; bufbusyl =0; return; };  //пакет не завершен символом "$" и отвергнут  =194=!!!!!!!!
                  procesreset();  scriptcancel();          //прекращаем выполнение предыдущего скрипта в т.ч. резидентного и выполняем экстренный
                  if( convert1(udpbuff, scriptp, scriptl) ) scripti=0;     //установка scripti в 0 запускает скрипт
                  bufbusyl = 0;         //признак того, что udp-буфер обработан
                  break;
//регулярный скрипт                  
      case '=' :  if( scripti >= 0) return;       //выполняется прeдыдущий скрипт;
                  if( udpbuff[bufbusyl-1] != '$')  
                    { procerror |= 128+64+2; bufbusyl =0; return; };  //пакет не завершен символом "$" и отвергнут  =194=!!!!!!!!
                  
                  if( convert1(udpbuff, scriptp, scriptl) ) scripti=0;
                  bufbusyl = 0;
                  break;
//резидентный скрипт
      case '>' :  if( scripti >= 0) return;       //выполняется прeдыдущий скрипт;
                  if( udpbuff[bufbusyl-1] != '$')  
                    { procerror |= 128+64+2; bufbusyl =0; return; };  //пакет не завершен символом "$" и отвергнут  =194=!!!!!!!!
                  if( convert1(udpbuff, scriptp, scriptl) ) {scripti=0; scrresident=1;}
                  bufbusyl = 0;
                  break; 
//добавочный скрипт
      case '%' :  if( scripti2 >= 0) return;       //выполняется прeдыдущий добавочный скрипт;
                  if( udpbuff[bufbusyl-1] != '$')  
                    { procerror |= 128+64+2; bufbusyl =0; return; };  //пакет не завершен символом "$" и отвергнут  =194=!!!!!!!!
                  if( bufbusyl > UdpPacMaxL2 ) {procerror |= 128+64+1;}; // Udp-пакет добавочного скрипта больше допустимого =193=!!!!!!!
                  
                  if( convert1(udpbuff, scriptp2, scriptl2) ) scripti2=0; 
                  bufbusyl = 0;
                  break;      
                  
 //     case '=' :  if( udpbuff[bufbusyl-1] != '$')  
 //                   { procerror |= 128+64+2; bufbusyl =0; return; };  //пакет не завершен символом "$" и отвергнут  =194=!!!!!!!!
 //                 convert2( udpbuff );
 //                 bufbusyl = 0;
 //                 break;
                  
      default :   procerror |= 128+64+32+4; bufbusyl =0; return;  //  пакет не идентифицирован по первому символу =228=!!!!!!!!!!!!!
    }
  }


    
//===================================SETUP======================================================================= 
#define TestPin1 7

void setup() {
  indx_t ch_i;
  
  pinMode(53, OUTPUT); //Написано, что так надо для работы с W5100
  pinMode(4, OUTPUT); digitalWrite(4, HIGH); //Отключаем SD-карту
  
  pinMode(TestPin1, OUTPUT); //тестовые средства
  digitalWrite(TestPin1, HIGH); // LED - Off
  //-------------------------------------
  gsBtns MnuBtns = gsBtns (2, {Btn1pin,Btn2pin});
  //------------------------------------- 
  pinMode(Btn1pin, INPUT); digitalWrite(Btn1pin, HIGH);
  pinMode(Btn2pin, INPUT); digitalWrite(Btn2pin, HIGH);
  
  lcd.init();  lcd.backlight(); lcd.cursor();     // stscr_init();
//  delay(1000);
//  lcd.noDisplay();
  
  Ethernet.begin(mac, ipaddr);
  Udp.begin(4000);

  ODmxp->set_control_pin(9);   // Arduino output pin for MAX485 input/output control (connect to MAX485-1 pins 2-3)
                               // pinMode выполняется в процедуре  
  ODmxp->set_tx_address(1);    // set rx1 start address
  ODmxp->set_tx_channels(DmxOChLast); // 2 to 512 channels in DMX512 mode.
  ODmxp->init_tx(DMX512);    // 

  IDmxp->set_control_pin(8);   // Arduino output pin for MAX485 input/output control (connect to MAX485-1 pins 2-3) 
  IDmxp->set_rx_address(1);    // set rx1 start address
  IDmxp->set_rx_channels(DmxIChLast); // 2 to 512 channels in DMX512 mode.
 // Установленное здесь количество каналов должно быть не более реального передаваемого пультом, иначе режим RxNotPermanent не включается !!
     // 
 
  procesreset();  //Очистка массивов конвейера
  projreset();    //Очистка массива проекций
  
   ODmxp->TxIntEn();
  IDmxp->init_rx(DMX512);
  
  timems = millis()+ Tick10ms * 10;
  }

//-----------------------------------------
void lcdprnt(unsigned int n, byte npos)
    { byte dig; unsigned int n1;
//      lcd.rightToLeft(); 
      if( npos >5) npos =5;
      for(byte i = 0; i < npos; i++) 
        { //n1 = n/10; dig = n - n1*10; 
         // lcd.write(48+dig);
          n = n1;
        }
//      lcd.leftToRight();
    }
    


//=========================================================================================================  
void loop() 
  { 
    if( millis() < timems ) {digitalWrite(TestPin1, LOW); return;} //ожидаем очередного тика
    digitalWrite(TestPin1, HIGH);      // Индикация свободного времени процессора
    timems = millis()+ Tick10ms * 10;
    
 //   gsbtn1.processing();
    btnprocessing();    //обрабатывает нажатия кнопок и управление меню
    if( wavetstgrgo )   //отработка второй полуволны WAVE-теста
      if( ! wavetsttime-- )   
         { ftstgrsendv( wavetstgrgo -1, tstvalues[wavetstgrgo -1][MinVal], tstvalues[wavetstgrgo -1][Tau01s] ); wavetstgrgo =0;}

    projection();          //отображение входных каналов на выходные
    busyflag=0;
         
//    scriptstep(scriptp);  
    scriptstep(scriptp, scripti, scriptl, scrresident, &scrktxreg);   //обработка текущей позиции регулярного скрипта
    scriptstep(scriptp2, scripti2, scriptl2, 0, &scrktxadd);      //обработка текущей позиции добавочного скрипта
    
    processtep();         //выдача команд в каналы, находящиеся в обработке
    
    ODmxp->TxIntEn();

    if( Udp.parsePacket() ) 
      { if(bufbusyl ==0) 
          { packnum++;                  //  lcd.setCursor(0,1); lcd.print(packnum);
            bufbusyl = Udp.available(); 
            if( bufbusyl > UdpPacMaxL ) {bufbusyl = UdpPacMaxL ;procerror |= 128+64+1;}; //слишком большой Udp-пакет(усечен) =193=!!!!!!!
            if( bufbusyl > UdpPacRecL ) {procerror |= 128+64+32+1;}; // Udp-пакет больше рекомендованного(508байт) ===========225=!!!!!!!
            Udp.read(udpbuff, bufbusyl);
            udpclear();
          } else   {  procerror |= 128+64+32+2;                   //буфер не освобожден к приходу очередного пакета =226=!!!!!!!!!!!!!
                      if(scrresident) { bufbusyl=0;}   //возможна потеря информации
                   }   
      }
    if( bufbusyl ) getpack();

    statescreen();
    
    IDmxp->RxIntEn();
  }

  /*    
   if(digitalRead(32)==LOW) button32=1;       //***инициализация
        else if (button32)
                { button31=0;
                  procesreset();  //Очистка массивов конвейера
                  projreset();    //Очистка массива проекций
                  for(int i=0; i < DmxOChLast; i++) *(ODmxp->TxBuffer+i)=0x0; //Обнуление выходного буфера 
                }

 
*/
