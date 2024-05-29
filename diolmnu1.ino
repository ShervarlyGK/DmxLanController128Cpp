//============================ MENU, Buttons & LCDscreen=====================================================
#include "diol1.h"

#define BtnLongPr  15 //* 0.1sec
#define Menu1showtm  40 //* 0.1sec
#define Menu1donetm  10 //* 0.1sec

gsBtns btns(2, {Btn1pin, Btn2pin});

byte wavetstgrgo =0;  //номер группы для которой запущен WAVE-test....
byte wavetsttime =0;  //счетчик времени в тиках для этого теста

enum FMenuCmd {m_show =1, m_sshow, m_go, m_tmout, m_done, m_bt1S, m_bt2S, m_bt2L};
byte btn1state=0, btn2state=0, inmenu1=0, inmenu2=0;
int btn1time, btn2time, menu1time=0;

//------------------------------------------menu 1--------------------------------------------------------------------------------------
enum Menu1item           {     GenerlCmd=1, Ini255Cmd, TstGr1cmd,    TstGr2cmd,  TstGr3cmd,  Menu1TOP };

enum Menu12item              {                           M12cmd1=1,         M12cmd2,        M12cmd3,         M12cmd4,         M12cmd5,         Menu12TOP};                              
static char tmenu1[][Menu12TOP][16]={{"General Cmd",    "Error Reset",      "-",            "-",             "-",              "-" },
                                     {"Ini255Ls Cmd",   "Clear List",       "Do Ini255",    "BlackOut List", "-",              "-"},
                                     {"TstGrp 1 Cmd",   "Clear List&Val",   "Send MIN Val", "Send MAX Val",  "MinMax WAVE",   "BlackOut Group"},
                                     {"TstGrp 2 Cmd",   "Clear List&Val",   "Send MIN Val", "Send MAX Val",  "MinMax WAVE",   "BlackOut Group"},
                                     {"TstGrp 3 Cmd",   "Clear List&Val",   "Send MIN Val", "Send MAX Val",  "MinMax WAVE",   "BlackOut Group"}};
//--------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------menu 2------------------------------------------------------------------------------------------------------------------------------
static char tmenu2[][16]={"","Initial 255 Chs", "IO Chan Transit", "Test Group 1", "TstGr1 Values", "Test Group 2", "TstGr2 Values",  "Test Group 3", "TstGr3 Values"};
enum Menu2item           {    Ini255set=1,       IOtransit,          TstGr1,         TstGr1v,         TstGr2,         TstGr2v,          TstGr3,         TstGr3v,   Menu2TOP };
static char tmenu2s[][16]={"","Ini255 Chs",     "IO Transit",       "Test Grp 1",   "TstGr1 Val",    "Test Grp 2",   "TstGr2 Val",     "Test Grp 3",   "TstGr3 Val"};

#define Ini255MaxN  10
#define TstGrMaxN   10
#define TransMaxN   8

enum TstGrIndx                {TstGR1i=0, TstGR2i, TstGR3i, TstGRiTOP};
byte ini255list [Ini255MaxN], testgr [TstGRiTOP][TstGrMaxN];
byte ini255n=0,               testgrn[TstGRiTOP];
byte trsourch, trdestlist [TransMaxN];

// enum TstGrValues {MinVal=0, MaxVal, Tau01s, GrValMaxN}; //GrValMaxN - это количество параметров группы- всегда последняя константа
// перенесено в header ****************
byte tstvalues[TstGRiTOP][GrValMaxN]={0,255,10, 0,255,10, 0,255,10};
static char tvalmenu [][16]={"MIN val=","MAX val=","Tm 0.1s="};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void menuerr() {}
//--------------------------------------------------------------------------

void ftstgrsendv(byte groupi, byte value, byte tau )
  { indx_t ch_i;
    if( testgrn[groupi] ==0 ) return;
    ch_i= dmxlgrinit( testgr [groupi][0], testgrn[groupi] , value, tau);
    for( byte i= 1; i < testgrn[groupi] ; i++ ) dmxlchadd(ch_i++, testgr [groupi][i]);
  }

//------------------------------------------------------------------------
byte ftstgrproc(byte groupi, byte cmd)
  { indx_t ch_i; byte value=0;
    switch( cmd )                   //"Clear List&Val", "Send MIN Val", "Send MAX Val", "MinMax WAVE",   "BlackOut Group"
      { case M12cmd1 : testgrn[groupi] =0; 
                       tstvalues[groupi][MinVal] =0; tstvalues[groupi][MaxVal] =255; tstvalues[groupi][Tau01s] =10; return 1;
           
        case M12cmd2 : value =tstvalues[groupi][MinVal]; ftstgrsendv(groupi, value, 0 );  return 1;
        case M12cmd3 : value =tstvalues[groupi][MaxVal]; ftstgrsendv(groupi, value, 0 );  return 1;
        
        case M12cmd4 : value =tstvalues[groupi][MinVal]; ftstgrsendv(groupi, value, 0 );
                       value =tstvalues[groupi][MaxVal]; ftstgrsendv(groupi, value, tstvalues[groupi][Tau01s] ); 
                       wavetstgrgo =groupi+1;   wavetsttime = tstvalues[groupi][Tau01s] *10 / Tick10ms;     return 1;
                       
        case M12cmd5 : ftstgrsendv(groupi, 0, 0 );  return 1;               
        default      : menuerr(); break;
      }
    return 0;
  }
  
//--------------------------------------------------------------------------
byte fini255proc(byte cmd)
  { indx_t ch_i; byte value=0;
    switch( cmd )                   //"Clear List",   "Do Ini255",    "BlackOut List",
      { case M12cmd1 : ini255n =0; return 1;  
      
        case M12cmd2 : value =255;
        case M12cmd3 : if( ! ini255n ) return 0;
                       ch_i= dmxlgrinit( ini255list [0], ini255n, value, 0);
                       for( byte i= 1; i < ini255n; i++ ) dmxlchadd(ch_i++, ini255list [i]); return 1;
                       
        default      : menuerr(); break;
      }
    return 0;
  }
  

//-------------------------------------------------------------------------
void fmenu1 (byte cmd)
  { static byte inmenu12, doneflag;
    byte gr_i;
    if(cmd == m_done ) {doneflag =1; lcd.setCursor(0,1); lcd.print("    **DONE**    "); menu1time = Menu1donetm *10 / Tick10ms; return;};
    if(cmd == m_tmout)
      { if( ! inmenu1 )   return;
        if( --menu1time ) return;
        inmenu1 = inmenu12 = doneflag =0;  return;  //выход из меню1
      }
    if( doneflag ) return;  
    if( !inmenu1 ) inmenu12 =0;  
    if( !inmenu12 )
      { switch (cmd)
          { case m_show  : 
            case m_bt1S  : if(++inmenu1 >= Menu1TOP ) inmenu1 =1;
                           menu1time = Menu1showtm *10 / Tick10ms; 
                           lcd.clear(); lcd.print(" MENU 1 ->"); lcd.setCursor(0,1); lcd.print(tmenu1[inmenu1 -1][0]);
                           return;
            case m_go    : 
            case m_bt2S  : case m_bt2L :
                           inmenu12 =1; fmenu1(m_show); return; 
          }  
      }
    else                  //второй уровень: inmenu12 != 0; 
      { switch (cmd)
          { case m_bt1S  : inmenu12++; if(inmenu12 >= Menu12TOP ) inmenu12 =1;
            case m_show  : menu1time = Menu1showtm *10 / Tick10ms; 
                           lcd.clear(); lcd.print("M1=>");  lcd.print(tmenu1[inmenu1 -1][0]);
                           lcd.setCursor(0,1);  lcd.print(tmenu1[inmenu1 -1][inmenu12]);
                           return;
            case m_bt2S  : case m_bt2L :
                           gr_i =TstGR1i;
                           switch( inmenu1 )
                             { case GenerlCmd : switch( inmenu12 )
                                                  { case M12cmd1 : procerror =0; fmenu1(m_done); return;
                                                    default      : break;
                                                  }
                                                break;  
                               case Ini255Cmd : if( fini255proc( inmenu12 ) ) {fmenu1(m_done); return;};
                                                break;    
                               case TstGr3cmd : gr_i++;
                               case TstGr2cmd : gr_i++;
                               case TstGr1cmd : if ( ftstgrproc(gr_i, inmenu12 ) ) {fmenu1(m_done); return;};
                                                break;
                               default        : break;
                             }
                           inmenu1 = inmenu12 =0; return; 
          }  
      } 
  }
  
//------------------------------------------------------  

void showlist( byte * list, byte i, byte col)     //вывод на lcd списка из i значений из массива *list; col - с какой позиции
   { char row1[16+5-col], *row1p; 
     row1p =row1; 
     while( i>0 && row1p <(row1+16-col) ) 
        { itoa( *( list+i-1 ), row1p, 10); row1p += strlen(row1p); i--;
          if( i ) { *row1p =','; row1p++; }
        }
     *row1p ='\0';
     for( char *chp = row1; chp < row1p; chp++ ) lcd.write(*chp); 
   }
//--------------------------------------------------------
void showgrval (byte grni, byte grvi)
  {
   lcd.setCursor(0,1); lcd.print(tvalmenu[grvi]);
   lcd.setCursor(9,1); lcd.print("    ");
   lcd.setCursor(9,1); lcd.print(tstvalues[grni][grvi]);
  }
  
//----------------------------- F MENU 2-----------------------------------------------------
void fmenu2 (byte cmd)
  { static byte menu2step, chnl, grni, grvi;      //menu2step - номер шага выполнения команды;
    byte val, gr_i;                               // 1-проведена инициализация экрана и алгоритма выполнения команды
    
    switch (cmd)
      { case m_show  : if(inmenu2 >= Menu2TOP ) inmenu2 =1;
                       menu2step =0; chnl=0;
                       lcd.clear(); lcd.print(" MENU 2 =>"); lcd.setCursor(0,1); lcd.print(tmenu2[inmenu2]);
                       return;
        case m_sshow : lcd.clear(); lcd.print("M2=>"); lcd.print(tmenu2s[inmenu2]); return;
                         
        case m_bt1S  : if( ! menu2step ) { inmenu2++;  fmenu2(m_show); return;};  // перебор команд меню2
                       switch (menu2step)  
                         { case  1 : switch( inmenu2 )
                                       { case Ini255set : case TstGr1 : case TstGr2  : case TstGr3 : 
                                                          if( chnl < DmxOChLast) chnl++; 
                                                          lcd.setCursor(0,1); lcd.print( chnl ); return;
                                         default        : break;
                                       }
                                     break;
                           case  2 : switch( inmenu2 )
                                       { case TstGr1v : case TstGr2v : case TstGr3v :
                                                        grvi++; if( grvi >= GrValMaxN) grvi=0; showgrval(grni, grvi); return;
                                         default      : break;
                                       }
                                     break;
                           case  3 : switch( inmenu2 )
                                       { case TstGr1v : case TstGr2v : case TstGr3v :
                                                        tstvalues[grni][grvi]++; showgrval(grni, grvi); return;
                                         default      : break;
                                       }
                                     break;  
                           default : break; 
                         }
        case m_bt2S  : if( ! menu2step )
                         { menu2step =1; fmenu2( m_sshow );
                           chnl =0; gr_i = TstGR1i;
                           switch( inmenu2 )                 //инициализация выполнения и отображения выбранной команды
                              { case Ini255set : lcd.setCursor(3,1); lcd.print(":");
                                                 showlist(ini255list, ini255n, 4);    //4=col - номер позиции на экране с которой выводим: 0,1,2...
                                                 lcd.setCursor(0,1); return;
                                                 
                                case IOtransit : lcd.setCursor(0,1); lcd.print("IChn= ");

                                case TstGr3    : gr_i++;                          
                                case TstGr2    : gr_i++;                
                                case TstGr1    : lcd.setCursor(3,1); lcd.print(":");
                                                 showlist(testgr[gr_i], testgrn[gr_i], 4);      lcd.setCursor(0,1); return;
                                                 
                                case TstGr3v   : gr_i++;                 
                                case TstGr2v   : gr_i++;                                                  
                                case TstGr1v   : grvi =MinVal; showgrval(gr_i, grvi); menu2step =2; return;
                                                                
                                default        : break;
                              }
                           return;
                         }
                       switch (menu2step)  
                         { case  1 : switch( inmenu2 )
                                       { case Ini255set : case TstGr1 : case TstGr2  : case TstGr3 : 
                                                          if( chnl+10 < DmxOChLast ) chnl +=10; else chnl =1;
                                                          lcd.setCursor(0,1); lcd.print( chnl ); return;
                                         default        : break;
                                       }
                                     break;
                           case  2 : switch( inmenu2 )
                                       { case TstGr1v : case TstGr2v : case TstGr3v :
                                                        lcd.setCursor(9,1); menu2step=3; return;
                                         default      : break;
                                       }
                                     break;
                           case  3 : switch( inmenu2 )
                                       { case TstGr1v : case TstGr2v : case TstGr3v :
                                                        tstvalues[grni][grvi]+=10; showgrval(grni, grvi); return;
                                         default      : break;
                                       }
                                     break;               
                           default : break; 
                         }
        case m_bt2L  : if( ! menu2step ) return;
                       gr_i = TstGR1i;
                       switch( inmenu2 )
                          { case Ini255set : if( ini255n >= Ini255MaxN ) { menuerr(); return;}
                                             ini255list[ ini255n++ ] =chnl;
                                             menu2step =0; fmenu2( m_bt2S ); return;
                                             
                            case TstGr3    : gr_i++;                 
                            case TstGr2    : gr_i++;                 
                            case TstGr1    : if( testgrn[gr_i] >= TstGrMaxN ) { menuerr(); return;}
                                             testgr[gr_i][ testgrn[gr_i]++ ] =chnl;
                                             menu2step =0; fmenu2( m_bt2S ); return;

                            case TstGr1v   : case TstGr2v : case TstGr3v :
                                             if(menu2step != 3)             return;
                                             menu2step=2; fmenu2(m_bt1S);   return;
                            default        : return;
                          }
      }
  }
  
//-------------------------------------------------------------------------------------------
void btn1short()
  { if( inmenu2 ) fmenu2( m_bt1S );
       else { fmenu1(m_bt1S);}
  }

void btn1long()
  { inmenu1 =0;
    if( inmenu2 ) {inmenu2 =0;  return;}     //выход из режима меню2...                   
    inmenu2 =1;  fmenu2(m_show);             //вход в меню2
  }

void btn2short()
  { if( inmenu2 ) { fmenu2( m_bt2S ); return; };
        
    if( inmenu1 ) {fmenu1( m_bt2S ); return;}   //выполнение пункта меню1 и выход
  }

void btn2long()
  { if( inmenu2 ) { fmenu2( m_bt2L ); return; };
    if( inmenu1 ) {fmenu1( m_bt2L ); return;}   //выполнение пункта меню1 и выход
  }
//------------------------------------------


//--------------------------------------------------------------------------  
void btnprocessing()
  { if(digitalRead(Btn1pin)==LOW) 
            switch ( btn1state )
                { case 0 : btn1state =1; btn1time=1; break;
                  case 1 : btn1time++; 
                           if(btn1time > BtnLongPr *10 /Tick10ms) {btn1state =2; btn1long();}
                  case 2 : break;
                }
       else switch ( btn1state )
                { case 0 : break;
                  case 1 : btn1short();
                  case 2 : btn1state =0; 
                }

    if(digitalRead(Btn2pin)==LOW) 
            switch ( btn2state )
                { case 0 : btn2state =1; btn2time=1; break;
                  case 1 : btn2time++; 
                           if(btn2time > BtnLongPr *10 /Tick10ms) {btn2state =2; btn2long();}
                  case 2 : break;
                }
       else switch ( btn2state )
                { case 0 : break;
                  case 1 : btn2short();
                  case 2 : btn2state =0; 
                }
    fmenu1( m_tmout );               
  }

//--------------------------------------------------------------------------
static char Versn[]="DIOL-127";

  void stscr_init()
    { lcd.clear();
      lcd.print(Versn);
      lcd.setCursor(9,0);lcd.print("Er=");
      lcd.setCursor(0,1);lcd.print("0    packs rcv");
      
//      lcd.setCursor(0,3);lcd.print("Loop= ");
    }
//----------------------------------------------------------------------------------------------
void statescreen()
  {  
   static byte inited, old_prcerr, old_busyf, blinkc;
   static int old_packn;
   
   if(++blinkc >20) blinkc=0;
   if( inmenu1 || inmenu2 ) { inited =0; return; }
   if( ! inited )
     { inited =1; stscr_init();}
     
   if( procerror != old_prcerr || inited ==1 ) {lcd.setCursor(12,0);lcd.print(procerror); old_prcerr =procerror;}
   if( packnum != old_packn || inited ==1 ) {lcd.setCursor(0,1); lcd.print(packnum); old_packn = packnum;}
   
   if( busyflag != old_busyf || inited ==1 ) { lcd.setCursor(15,1); 
                                               if(busyflag ) lcd.print(">"); else lcd.print(" "); 
                                               old_busyf = busyflag;}
   inited =2;

   
//    lcd.setCursor(12,1);lcd.print(scripti); lcd.print("   ");
//    if(inloop) {lcd.setCursor(6,3);lcd.print(loopcount); lcd.print("   ");}
//      else {lcd.setCursor(6,3);lcd.print("       ");};
//    lcd.setCursor(9,3);  lcd.print("        ");
 //   lcdprnt(scripttm,5); 
 //   for(byte i=1; i <8; i++ ) lcd.write(32);
  }
  