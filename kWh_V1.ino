  /*----------------------------------------Cargar Librerías--------------------------------------------*/
  #include <SPI.h>              //  Librería para comunicación SPI
  #include <Wire.h>             //  Librería wire para la comunicación I2C entre el DS1307 y El Arduino
  #include <UTFT.h>             //  Librería UTFT para el manejo de la TFT
  #include <RTCtft.h>           //  Librería para el RTC
  /*---------------------------------------Declaración de variables Globales------------------------------------------*/

  //  Variables tipo caracter para el RTC en la TFT
  char hours[6], minutes[6], segundos[6];            //  Variables para mostrar el tiempo          
  char days [6], months [6], years   [6];            //  Variable para mostrar la fecha
  
  //  Varibles tipo caracter para mostrar los parámetros de energía en la tft.
  char Irmss[6], Vrmsv[6], PowerReal[10], FacPot[10], PowerAparent [10];  //  Alias para mostrar las variables eléctricas
  char EnergyTotal [10], EnergyDay [6], EnergyMonth [6], costo1 [6];     //  Variables para mostrar la energía y el costo de consumo
  char SendEnergy[6], SendCosto[6];
  
  //  Asignamos vlores constantes para calibración y el tiempo de muestreo
  #define analogPinV 0                               //  Declraro el pin (A0) para sensar  voltaje
  #define analogPinI 1                               //  Declraro el pin (A1) para sensar corriente    
  #define PhaseCalib 0.939                           //  Constante de calibración de fase
  #define timeEnd 2000                               //  Tiempo de  espera para muestrear 
  #define crossings 12                               //  Numero de cruces por cero a muestrear
  #define SupplyVolt  5.02                           //  Voltaje de alimentación (Necesario para calibrar)
  #define VCalib ((77.6 * SupplyVolt)/ 1024)         //  Formula para la calibración de voltaje
  #define ICalib ((56.5* SupplyVolt)/ 1024)          //  Fórmula para la calibración de corriente
  #define powerCalib (VCalib*ICalib)                 //  Fórmula para la calibración de potencia
  
  //  Variables para tomar las muestras del ADC del arduino 
  int AnalogCurrent = 0, AnalogVolt = 0;             //  Variables para sensar corriente y voltaje      
  int AntMuestraV, AntMuestraI;                      //  Variables para guardar las muestras pasadas
  int muestraV, muestraI;                            //  Variables que toman las muestras actuales
  
  //  Varibles para almacenar las muestras actual y anterior filtradas  
  double FilterInitV , FilterUltV;                         //  Variables para guardar las muestras filtradas pasadas
  double FilterInitI , FilterUltI;                         //  Variables para guardar las muestras filtradas actuales
  
  //  Variables para realizar la sumatoria de las muestras 
  double sqrV,sumV,sqrI,sumI,sumP, instP, correctPhaseV;   //  Variables para guardar la sumatoria de las muestras                     
  int Vrms;                                                //  Alias que toma el voltaje RMS
  float Irms, powerFactor, realPower,aparentPower;         //  Alias que toman las variables de energía eléctrica
  double whDay=0, kwhMonth =0, kwhTotal =0, costo= 0;      //  Inicialización de los valores de la energía consumida
  boolean AntVCross, checkVCross, evaltime;                //  Variables para establecer una sentecia (true or false)
  

  //  Variables de tiempo para calcular la energía diaria mensual y acomulada.
  unsigned long fast_update, fast_update1,fast_update2;
  unsigned long startTime, startTime1, startTime2;
  unsigned long tmillisInit, tmillis, difTmillis;
  int hour = 0, minute = 0;
  int hourReset, dayReset, minutReset, segunReset;
  
  extern uint8_t SmallFont[]; //Fondo de Pantalla pequeño
  extern uint8_t BigFont[];   //Fondo de pantalla mediano 
 
  RTC_DS1307 RTC;                        //Inicialización de funciones para el RTC       
  UTFT myGLCD(ITDB32WD, 38, 39, 40, 41); //Inicia la Pantalla TFT(Model, RS, RW, CS, RST)) 
 // UTouch myTouch(6,5,4,3,2);

  /*-----------------------------------------Inicialización de la TFT--------------------------------------------------*/
  
  void InitTft (){
  myGLCD.fillScr(VGA_TEAL);
  myGLCD.setBackColor(VGA_WHITE); //Seleccionamos el color de fondo de la tft
  myGLCD.setColor(VGA_NAVY);     //Seleccionamos el color de fuente para la tft
  myGLCD.setFont(BigFont);          //Seleccionamos el tamaño de fuente
  myGLCD.print("SMART METER",CENTER, 0); 
  myGLCD.setBackColor(VGA_SILVER); //Seleccionamos el color de fondo de la tft
  myGLCD.setColor(VGA_SILVER);
  myGLCD.fillRect(10,20,146,170);
  myGLCD.fillRect(177,20,375,170);
  myGLCD.setColor(VGA_MAROON);
  }
  
  /*--------------------------------------Inicializo RTC-----------------------------------------------------------------*/
  void rtc(){
  DateTime now = RTC.now(); 
  sprintf(days, "%02d", now.day());
  myGLCD.print(days, 100, 200);  
  myGLCD.print("/", 130, 200);
 
  sprintf(months, "%02d", now.month());
  myGLCD.print(months, 148, 200);  
  myGLCD.print("/", 178, 200);
  
  sprintf(years, "%04i",now.year());
  myGLCD.print(years, 196, 200);
        
  sprintf(hours, "%02d", now.hour()); 
  myGLCD.print(hours, 152, 220);
  myGLCD.print(":", 180, 220);
  
  sprintf(minutes, "%02d ",now.minute());
  myGLCD.print(minutes, 192, 220);
  
  sprintf(segundos, "%02d ",now.second());
  myGLCD.print(segundos, 234, 220);
  }

 /*-----------------------------Calculo de la Energía--------------------------------------------------------------------------*/

  void calcEnergy() 
  {
  int CrossCount = 0;                                     //  Inizializamos el # de cruces por 0                    
  int NumMuestras = 0;                                    //  Inizializamos el # de muestras 
  evaltime = false;                                       //  Evaluamos el tiempo y asignamos un falor falso
  startTime = millis();                                   //  Evalua el tiempo desde que se enciende el arduino  
  
  //  Ciclo while para detectar el primer cruce por 0
  while(evaltime == false)   {                                        //  Condición para entrar en el ciclo while     
    AnalogVolt  = analogRead(analogPinV);                             //  Lee  la señal de voltaje del ADC
      if ((AnalogVolt  < 550) && (AnalogVolt  > 440)) evaltime=true;  //  Chequea si la señal esta cerca de un cruce por 0 (512 del ADC), si es así sale del ciclo while
      if ((millis()-startTime)>timeEnd) evaltime = true;              //  Si se produce un error se asegura que no quede en un ciclo infinito
      }

  //Una vez que detecta el primer cruce por cero Realiza la sumatoria de las muestras de 12 cruces por 0 y luego se saca el promedio 
  startTime = millis(); 
  while ((CrossCount < crossings) && ((millis()-startTime)<timeEnd)) 
      {
    NumMuestras++;                                             //  Contador para incrementar el número de muestras
    AntMuestraV=muestraV;                                      //  La muestra actual de corriente se convierte en una muestra pasada
    AntMuestraI=muestraI;                                      //  La muestra actual de voltaje se convierte en una muestra pasada
    FilterInitV = FilterUltV;                                  //  La muestra  de voltaje filtrada acual se convierte en muestra pasada
    FilterInitI = FilterUltI;                                  //  La muestra  de corriente filtrada acual se convierte en muestra pasada
    muestraV = analogRead(analogPinV);                         //  Empieza a tomar muestras de voltaje
    muestraI = analogRead(analogPinI);                         //  Empieza a tomar muestras de corriente
    
    //  Aplicación de filtros digitales para eliminar el nivel de DC
    FilterUltV = 0.996*(FilterInitV+(muestraV-AntMuestraV));   //  Filtro pasa alto de para la señal de voltaje
    FilterUltI= 0.996*(FilterInitI+(muestraI-AntMuestraI));    //  Filtro pasa alto de para la señal de corriente
    sqrV= FilterUltV * FilterUltV;                             //  Se eleva al cuadrado las muestras de voltaje filtradas 
    sumV += sqrV;                                              //  Sumatoria de las muesras de voltaje
    sqrI = FilterUltI * FilterUltI;                            //  Se eleva al cuadrado las muestras de corriente filtradas 
    sumI += sqrI;                                              //  Sumatoria de las muesras de corriente
    
    // Se Realiza la correción de fase tomando en cuenta las constantes de calibración 
    correctPhaseV = FilterInitV + PhaseCalib * (FilterUltV - FilterInitV);  
    instP =abs(correctPhaseV * FilterUltI);                   //  Se saca el valor absoluto para que no afecte el sentido de los sensores     
    sumP +=instP;                                             //  Sumatoria de la potencia instantanea
  
   //  Se evalúa el número de cruces por 0 (Cada 2 cruces una onda completa)   
    AntVCross = checkVCross;                                  //  El # de cruce actual se convierte en el anterior
        if (muestraV > AnalogVolt) checkVCross = true;        //  Se evalúa si existe un cruce por 0 y se asigna un valor true  
       else checkVCross = false;                              //  Si no es así la varible sigue en false y el contador no aumenta  
        if (NumMuestras==1) AntVCross = checkVCross;          //  Si es la primera muestra el # de cruce por 0 actual es igual al anterior              
        if (AntVCross != checkVCross) CrossCount++;           //  Si el Cruce por 0 anterior es diferente de cruce actual aumenta el contador
      }
 
  //  Se realiza operaiones finales para mostrar los valores de Vms, Irms RealPower, Aparentpower
    Vrms = VCalib * sqrt(sumV / NumMuestras);                 //  Calculo de voltaje eficaz
        if (Vrms <= 1){ Vrms=0;}                              //  Si no existe voltaje se asegura que no arroje valores erroneos
    Irms = ICalib * sqrt(sumI / NumMuestras);                 //  Calculo de corriente eficaz
        if (Irms <= 0.13){ Irms=0;}                           //  Se asegura que no mida valores de corriente erroneos porducidos por ruido
    realPower = (powerCalib * sumP) / NumMuestras;            //  Calculo de la potencia real
        if (realPower <= 14){ realPower=0;}                   //  Si los niveles de corrienten corresponden a ruido no mide la potencia
    aparentPower = Vrms * Irms;                               //  Calculo de la potencia aparente
    powerFactor=(realPower / aparentPower);                   //  Calculo del factor de potencia
        if (powerFactor > 1 ){ powerFactor=1;}                //  Si se produce un error a la entrada de las señales se asegura que el factor de potencia no se exceda
      
  //  Se reestablece la sumatoria para volver a muestrear las señales
  sumV = 0;                                         
  sumI = 0;
  sumP = 0;
 
   }

/*-------------------------------------Función principal del programa-----------------------------------------------------*/

  void setup(){
  Serial.begin(9600);
  RTC.begin();
  Wire.begin(); 
  RTC.adjust(DateTime(__DATE__,__TIME__)); //Habilitar esta  línea para ajustar hora actual luego poner en comentarios y cargar para que se guarden los cambios         
  myGLCD.InitLCD();
  myGLCD.clrScr();
  InitTft ();

  startTime1= millis()+9294;
  
    }

   
/*-------------------------------------------------Programa Principal---------------------------------------------------------*/

void loop(){  
 
   rtc();                                                      //  Se inicializa el RTC
   calcEnergy();                                               //  Llma a la función del calculo de energía
  
  //  Tiempo de espera para que se esbilize la placa Arduino       
      if( (long)( millis() - startTime1) <= 0)                
        {
        Vrms=0;Irms=0;realPower=0;aparentPower=0;powerFactor=0;
        }
        
  //  Calculo de la enrgía Consumida
      if ((millis()-fast_update)>1000)               //  Evaluacoón de potencia por cada segundo (Energía Juls)
      {
      fast_update = millis();                        //  El tiempo actual se guarda en una variable para su posterio evaluación 
      DateTime now = RTC.now();                      //  Se inicializa el RTC para programar el tiempo la hora de reestablecimiento de la energía
      int last_hour = hourReset;                     //  Se guarda la hora actual en una variable para poder compararla 
      hourReset = now.hour();                        //  Se guarda la hora actual
      dayReset=now.day();                            //  Se guarda la fecha del día actual
      whDay += (realPower * 1) / 3600;               //  Cálculo de la Energía diaria consumida (En Wh)
      kwhMonth += (realPower*1)/3600000;             //  Cálculo de la Energía mensual consumida (En kWh)
      kwhTotal += (realPower*1)/3600000;             //  Cálculo de la Energía total consumida (En kWh)
      
  //  Se Programa una determinada hora y día para reestablecer los valores de energía medidos
        if (last_hour == 23 && hourReset == 00){ whDay = 0;  }                        //  Se reestablee el conteo de Wh diario a la media noche
        if (dayReset == 11 && (last_hour == 23 && hourReset == 00) ){kwhMonth=0;}     //  Se reestablece el conteo de kWh mensual el 11 de cada mes a media noche
      } 
      
  costo = 0.08*kwhMonth;                             //  Se calcula el costo por kWh
    visualization ();                                //  Se envía los valores a la tft para su visualización
  
  //delay(1000);
  
 }
 
 
  //  Creación de una función para la vizualización      
  void visualization (){
  myGLCD.print("I=", 10, 20);
  dtostrf(Irms, 4, 1, Irmss);
  myGLCD.print(Irmss, 50, 20);
  myGLCD.print("A", 115, 20);
  
  myGLCD.print("V=", 10, 40);
  dtostrf(Vrms, 3, 0, Vrmsv);
  myGLCD.print(Vrmsv, 50, 40);
  myGLCD.print("V", 100, 40);
  
  myGLCD.print("P=", 10, 60);
  dtostrf(realPower, 5, 0, PowerReal);
  myGLCD.print(PowerReal, 50, 60);
  myGLCD.print("W", 130, 60); 
   
  myGLCD.print("S =", 180, 20);
  dtostrf(aparentPower, 5, 0, PowerAparent);
  myGLCD.print(PowerAparent, 230, 20);
  myGLCD.print("VA", 315, 20);   
  
  myGLCD.print("FP=", 180, 40);
  dtostrf(powerFactor, 4, 3, FacPot);
  myGLCD.print(FacPot, 230, 40);
   
  myGLCD.print("ED=", 180, 60);
  dtostrf(whDay, 4, 1, EnergyDay);
  myGLCD.print(EnergyDay, 230, 60);
  myGLCD.print("Wh", 310, 60);
  
  myGLCD.print("EM=", 180, 80);
  dtostrf(kwhMonth, 6, 1, EnergyMonth);
  myGLCD.print(EnergyMonth, 230, 80);
  myGLCD.print("kWh", 328, 80);
  
  myGLCD.print("ET=", 180, 100);
  dtostrf(kwhTotal, 6, 1, EnergyTotal);
  myGLCD.print(EnergyTotal, 230, 100);
  myGLCD.print("kWh", 328, 100);
     
  myGLCD.print("COSTO=", 70, 140);
  dtostrf(costo, 5, 2, costo1);
  myGLCD.print(costo1, 195, 140);
  myGLCD.print("$", 170, 140);
   }
  
