#include <Arduino.h>
/* SMS Manager for Power monitoring

  Uses GSM Library GPRS_Shield_Arduino.h v2 (Non Suli)

  Specifiche:
  - OK    Verifica corretta registrazione
  - OK    Reinizializzazione ogni 24 ore (ogni giorno) ad ora prestabilita
  - OK    invio SMS mancanza energia fino a 3 numeri
  - OK    invio SMS riattivazione energia fino a 3 numeri
  - OK    Comando SMS (S) per stato Power Supply e SMS a numero richiedente
          + Messaggio SMS per richiedente non autorizzato 
  - OK    Salvataggio stato in memoria non volatile (EEPROM O FLASH)
  - Prevedere la richiesta SMS per vedere quanti e quali numeri sono impostati
  - NO   (disabilitazione o abilitazione notifica a numero da richiesta SMS)
  - (disabilitazione o abilitazione notifica a TUTTI i numeri da richiesta SMS)
  - set/reset pin uscita da SMS numero richiedente abilitato
  - stato pin ingresso su richiesta SMS a numero richiedente abilitato

  - Prevedre SMS di conferma comandi (richiesta eseguita per il n.)


  Comandi SMS
  Numeri GSM 
  Italia    totale cifre 12 senza +
  Germania  totale cifre 13 senza +
  Svezia    totale cifre 11 senza +
  Finlandia totale cifre 10 senza +

  "M+393391255597" Sostituisci cellulare autorizzato Master con un altro
  "A1+393391255597" AGGIUNGI/SOSTITUISCI cellulare in posizione....
  "D" CANCELLA tutti i cellulari ausiliari (non il Master)

  "N" Elenca tutti i numeri autorizzati - solo per il Master
  "E" Cancella tutti i numeri incluso il Master - NON DOCUMENTATO. Accetta qualsiasi numero.

  EEPROM.read(5) Indicatore Rete presente (0) Rete assente (1)

  1st Power On
  -  inizialmente la EEPROM non ha numeri (tutti "") 
  -  Definire il numero Master (M)
  -  Una volta definito il numero master è possibile definire il primo numero ausiliario (A1)
     e poi il secondo numero Ausiliario (A2)
  -  Successivamente con "D" si possono CANCELLARE tutti i cellulari eccetto il numero Master autorizzato (0)    

  SoftwareSerial library Notes
  With Arduino 1.0 you should be able to use the SoftwareSerial library included with the distribution (instead of NewSoftSerial).
  However, you must be aware that the buffer reserved for incoming messages are hardcoded to 64 bytes in the library header,
  "SoftwareSerial.h": 1.define _SS_MAX_RX_BUFF 64 // RX buffer size
  This means that if the GPRS module responds with more data than that, you are likely to loose it with a buffer overflow!
  For instance, reading out an SMS from the module with "AT+CMGR=xx" (xx is the message index), you might not even see the message part because
  the preceding header information (like telephone number and time) takes up a lot of space.
  The fix seems to be to manually change _SS_MAX_RX_BUFF to a higher value (but reasonable so you don't use all you precious memory!)
  The Softwareserial library has the following limitations (taken from arduino page)
  If using multiple software serial ports, only one can receive data at a time.
  http://arduino.cc/hu/Reference/SoftwareSerial This means that if you try to add another serial device ie grove serial LCD you may get communication errors
  unless you craft your code carefully.

  AT+CLTS    Get Local Timestamp
  Test Command AT+CLTS=? Response +CLTS: "yy/MM/dd,hh:mm:ss+/-zz" OK
  (verificare differenza con AT+CCLK?)
  Read Command AT+CLTS?  Response +CLTS:<mode>	OK
  WriteCommand AT+CLTS=<mode>  Response OK	ERROR
  Mode 0 Disable		1 Enable

  Set and Save AT+CLTS=1;&W Response OK
  Restart the modem using Software Restart command AT+CFUN=1,1
  Response OK	RDY
  +CFUN: 1 +CPIN: READY
  Call Ready
  PSUTTZ: 2016,12,28,10,30,7,"+22",0 (unsolicited)
  DST: 0
  SMS Ready
  AT+CLTS? Response +CLTS 1
  OK (enabled)
  AT+CCLK? Response +CCLK: "16/12/28,16:01:27+22" OK

  Serial.println(F("String"));
*/

#include "GPRS_Shield_Arduino.h"
#include "EmonLib.h"
#include "EEPROM.h"

#define PIN_TX    2
#define PIN_RX    3
#define PIN_RST   7
#define BAUDRATE  9600
 /* M. Veneziano 2020 Voltage calibration Transformer + Partition. Set for each specific transformer */ 
#define VOLT_CAL 136.0 
#define MESSAGE_LENGTH 160
#define BUFFER_LENGTH 50
char message[MESSAGE_LENGTH]; // Message = 160 Char
char locDateTime[BUFFER_LENGTH];// Local Date and Time = 50 Char
/* Giorno, Ora e Minuto
Reset Day (rday) a 0 perchè diverso da 1 e da 31 e quindi 
consentirebbe il reset fin dalla prima ora giusta del primo giorno */
int day, rday=0, hh, mm;
//Ora e Minuto di Reset giornaliero
int ORAr = 03, MINr = 00;
int size,Auxnphones,phoneI;
//char EStr[20];  // buffer fisso da 20 caratteri
int Auth;
char EStr[4][20];

//          VARIABILI
//phoneT => phone Temporaneo
//Auxnphones => numero di phones ausiliari
//phoneI => Index phone (escluso Autorizzato (0) )
//EStr => buffer EEPROM character array
//Auth => 0= numero non autorizzato 1= numero autorizzato

int messageIndex = 0;
float PowerVoltage;
char phone[16], phoneT[16];

char datetime[24];
//char *phoneAut[] = {"+393334188263","+393383418818", "+393391255597",""};
//char *phoneAut[] = {"+393460607220","+393383418818", "",""};
//char *phoneAut[] = {"+393460607220","", "",""};
//char *phoneAut[] = {"","","",""}; // Condizione Iniziale


// Precarica il primo numero

// Se devi modificarle a runtime, poi nel codice scrivi qualcosa tipo phoneAut[1] = "+390000000000"; o modifichi i contenuti, allora devi usare buffer modificabili:

//char phoneAut[][20] = {
//    "+393334188263",
//   "",
//   "",
//   ""
//};
// In questo modo ogni cella ha spazio per una stringa di max 19 caratteri (più il terminatore \0).


//A char *phoneAut[] = {"+393334188263","","",""};
//B const char *phoneAut[] = {"+393334188263","","",""};
//C

char phoneAut[][16] = {
    "+393334188263",
    "",
    "",
    ""
};
// In questo modo ogni cella ha spazio per una stringa di max 15 caratteri (più il terminatore \0).

// phoneAut[0] authorized master number - Can authorize up to 4 phone numbers - "" used as terminator


char outmessage[50];
//char testo[50];
uint32_t iniTime, passTime;	// Valore Tempo iniziale, Valore tempo trascorso
uint32_t previousMilliscc = 0, previousMillisora = 0;
uint32_t intervalcc = 10000; //intervallo per il controllo del valore di tensione attuale - 10 sec
uint32_t intervalora = 900000; //intervallo per il controllo dell'ora - 15 Minuti - 3600000 1 ora
//uint32_t intervalora = 120000; //intervallo per il controllo dell'ora - 2 Minuti - 3600000 1 ora

//COOP INFO SMS (Credito residuo)
#define INFO_NUMBER "4243688"
#define INFOTXT  "SALDO"
//#define INFOTXT "INFO SIM"



GPRS gprs(PIN_TX, PIN_RX, BAUDRATE); //RX,TX,BaudRate
EnergyMonitor emon1;	//Initialize EnergyMonitor ?



//   ============  F U N Z I O N I ======================

void initgsm()
  {
  //Initialize Modem and emoncms

  Serial.begin(9600);
  //Serial.println("Program started...\nStarting Power On sequence");
  Serial.print(F("Program started...\nStarting Power On sequence\n"));

  
  emon1.voltage(0, VOLT_CAL, 1.7);  // Defines Voltage: input pin, Voltage calibration, phase_shift
  //emon1.current(0, 32);
  for (int i = 0; i < 5; i++)
   { //clean up data
    emon1.calcVI(20,2000); // Run 20 measurement made of 20 halfwave with a 2000ms Timeout
   }

  // Start GSM Modem Reset
    while (!gprs.init())
    {
    gprs.powerUpDown(PIN_RST); //PIN_RESET (7) - RESET Modem
    delay(1000);
    }
  delay(1000);
 
  Serial.print(F(" - Init Success - Completed GSM Power On Sequence - Reset\n"));
  iniTime = millis(); // Valore Tempo iniziale
  
// Garantisce che il Modem sia registrato sulla rete
  while (!gprs.isNetworkRegistered())
    {
        delay(1000);
        Serial.print(F("Network has not registered yet!\n"));
    }
  Serial.print(F("GSM network initialization done!\n"));

  delay(500);

  //              PREVENTIVELY DELETE ALL SMS UNREAD
  // Determines the n. of received SMS Unread  
  messageIndex = gprs.isSMSunread();
    delay(2000);

  for (int i = messageIndex; i > 0; i--)
    {
        gprs.readSMS(i, message, MESSAGE_LENGTH, phone, datetime);
        delay(2000);

//In order not to full SIM Memory, is better to delete it
        gprs.deleteSMS(i);
        delay(2000);
    } 
    
  // Invia SMS al numero Coop Voce 42 43 688 INFO SIM per credito residuo
  // in modo da ricavare la data e l'ora corrente
  Serial.print(F("Invio Messaggio INFO\n"));

  if (gprs.sendSMS(INFO_NUMBER, INFOTXT))
    { // Send SMS to defined phone number and text
        Serial.print(F("Send SMS Succeed!\r\n"));
        Serial.flush();
    } 
  else
    {
      Serial.print(F("Send SMS failed!\r\n"));
      Serial.flush();
    }

  //                   Legge i messaggi INFO ricevuti
  // C'è il rischio che si frapponga un SMS di servizio del provider
  // Solo se non passa molto tempo dalla registrazione alla rete
  // (Vedi cancellazione preventiva)
    Serial.print(F("Legge i messaggi ricevuti"));
  // Determines the n. of received SMS Unread
    messageIndex = gprs.isSMSunread();
    delay(2000);

  // Legge tutti gli SMS e usa i dati del primo
  for (int i = messageIndex; i > 0; i--)
    {
    gprs.readSMS(i, message, MESSAGE_LENGTH, phone, datetime);
    delay(2000);
    //In order to not full SIM Memory, is better to delete it
    gprs.deleteSMS(i);
    delay(2000);
    } 

    Serial.print("From number: ");
    Serial.println(phone);
    Serial.flush();
    Serial.print("Datetime: ");
    Serial.println(datetime);
    Serial.flush();
    Serial.print("Received Message: ");
    Serial.println(message);
    Serial.flush();

    // RTC Network Time updating is disabled
    sim900_check_with_cmd(F("AT+CLTS=0\r\n"), "OK", CMD);

    /* Set RTC time to i.e (datetime). 
    "yy/MM/dd,hh:mm:ss+/-zz"
    zz quarter (-47....+48)of hour between local time and GMT
    6 maggio 2010,00:01:52 GMT +2 ore
    "10/05/06,00:01:52+08
    */ 
    sprintf (outmessage, "AT+CCLK=\"%s\"\r\n", datetime);
    Serial.println (outmessage);
    Serial.flush();
    if (sim900_check_with_cmd (outmessage, "OK", CMD))
      {
        Serial.println ("A buon Fine");
        Serial.flush();
      }
      else
        {
            Serial.println ("Non a buon Fine");
            Serial.flush();
    // If effetti qui bisognerebbe gestire che l'SMS sia un vero messaggio di data
        } 

   //AT+CLTS=1	// Enable date and time from network - WIND Funziona COOP VOCE NON FUNZIONA
  
  // sim900_check_with_cmd(F("AT+CLTS=1\r\n"), "OK", CMD);
  // delay(2000);

  // Sync date time GSM RTC to Network
  //sim900_check_with_cmd(F("AT+CCLK?\r\n"), "OK", 0);
  //	AT+CCLK?						--> 8 + CR = 9
  //+CCLK: "14/11/13,21:14:41+04"	--> CRLF + 29 + CRLF = 33
  //
  //OK							--> CRLF + 2 + CRLF =  6
  /*
    "yy/MM/dd,hh:mm:ss+/-zz"
    zz quarter (-47....+48)of hour between local time and GMT
    6 maggio 2010,00:01:52 GMT +2 ore
    "10/05/06,00:01:52+08
  */

 // Ricava da RTC Data e ora e li pone in locDateTime
  gprs.getDateTime(locDateTime);
  Serial.print(" Data e Ora da rete: ");
  Serial.println(locDateTime);

//AT+CMGF=1	// Enable ASCII TEXT mode for SMS
  if (sim900_check_with_cmd(F("AT+CMGF=1\r\n"), "OK", CMD))
    {
// Set message mode to ASCII
      Serial.println(" Set ASCII TEXT mode for SMS....");
    }

   delay(500);  
}

//   ============  A L T R E   F U N Z I O N I ======================

void writeString(int offs, const char *edata)
  {
    int i = 0;
    while (edata[i] != '\0' && i < 20) {
    EEPROM.write(offs + i, edata[i]);
    i++;
  }
  EEPROM.write(offs + i, '\0'); // terminatore
  }

void read_String(int offs, char *dest)
  {
    int len = 0;
    unsigned char k;

  do
    {
        k = EEPROM.read(offs + len);
        dest[len] = k;
        len++;
    }
  while (k != '\0' && len < 20);

  dest[len - 1] = '\0'; // assicurati che termini con \0
  }

bool TimeToReset () {
// Reset Now?

//    locDateTime - String like "24/05/29,10:30:15+08" "yy/MM/dd,hh:mm:ss+/-zz"

  day = (locDateTime[0] - '0') * 10 + (locDateTime[1] - '0');
  hh = (locDateTime[10] - '0') * 10 + (locDateTime[11] - '0');
  mm = (locDateTime[13] - '0') * 10 + (locDateTime[14] - '0');

  Serial.print("ORA, MIN --> ");
  Serial.print(hh);
  Serial.print(":");
  Serial.print(mm);
  Serial.println(" <--");
  if (hh == ORAr && (day != rday)) {
    rday = day;
    return true;
    // Esegui Reset ed aggiorna il giorno di Reset (domani)
  }
  return false;
  // Non è ancora l'ora di Reset 
}

void calc()
  {
    emon1.calcVI(20,2000);  // Calculate Emoncms parameters - Measurement on 20 halfwave with a 2000ms Timeout
    PowerVoltage   = emon1.Vrms;  //extract Vrms into Variable
  //irms[0] = emon1.calcIrms(4000);  // Calculate Irms only
  //irms[1] = irms[0] * 225.0;
  }

void CalcNphone() {
  // POTREBBE NON SERVIRE
  //Loop - Calculates number of auxiliry phones (Auxnphones) basandosi sulla lunghezza della stringa. 0 significa stringa vuota
  // con "break" esce dal loop con "i" che ha contato l'indice (che parte da 0)
  // di quante stringhe di telefoni ausiliari c'erano.
  // ATTENZIONE ! Nel caso di Auxnphones=0 
  // vale anche in presenza/assenza del numero Master
  // Quindi in realtà indica il numero di numeri An ausiliari
  /*
  phoneAut[0]=Master
  phoneAut[1]=A1
  phoneAut[2]=A2
  phoneAut[3]=A3
  */
  
  for (int i = 0; i < 4; i++) {
       if (strlen(phoneAut[i]) == 0) {
        Auxnphones=i;
        break;
      }
   }
  }

void RestorePhones()
 {
  // At INIT copy EEPROM (EStr) to phoneAut
  // ======== NO ======== At RUNTIME copy phoneAut to EStr (EEPROM)

  // If in EEPROM the first char of an entry (phone) is '+' it is considered that a phone is loaded
  // Otherwise load the predefined number as defined in RAM (phoneAut).
  // EStr used to save EEPROM writing (16 char single phone)

  // Scan and Read from EEPROM content (authorized phone numbers, Master included) and copy the content to EStr 
  // Auxiliary numbers are set only if a MASTER number is present (phoneAut[0] = Master number)
  Auxnphones = 0;
    for (int i = 0; i < 4; i++)
    {
        read_String(6 + i*17, EStr[i]); // nuova funzione che legge la EEPROM e riempie un buffer char[] - elemento "i"
        if (EStr[i][0] == '+')
// Checks the presence of a Pnone number (*) in EEPROM. If not breaks the loop and set Auxnphones to i
        {
            strcpy(phoneAut[i], EStr[i]);
            Auxnphones = i;
// Copy the string from EStr to phoneAut
// Now the content in RAM(phoneAut)=EEPROM=EStr
// Counts valid phones
        } 
        else 
        {
        break;
// Empty position
        }
    Serial.print ("Autorized phone n.");
    Serial.print (i);
    Serial.print (": ");
    Serial.println (EStr[i]);
   } 
  Serial.print ("Numero di telefoni ausiliari + Master: ");
  Serial.println(Auxnphones+1);
}

void PhoneInit() {
  // At INIT copy EEPROM (EStr) to phoneAut
  // ======== NO ======== At RUNTIME copy phoneAut to EStr (EEPROM)

  // If in EEPROM the first char of an entry (phone) is '+' it is considered that a phone is loaded
  // Otherwise load the predefined number as defined in FLASH (phoneAut).
  // EStr used to save EEPROM writing (16 char single phone)

  // Scan and Read from EEPROM content (authorized phone numbers, Master included) and copy the content to EStr 
  for (int i = 0; i < 4; i++) {
      read_String(6 + i*17, EStr[i]);  // nuova funzione che legge la EEPROM e riempie un buffer char[] - elemento "i"

      if ((EStr[i][0] != '+') && (strlen(phoneAut[i]) == 0)) {
    // Checks a empty position in EEPROM and in RAM If yes breaks the loop and set Auxnphones to i
        Auxnphones=i;
        break;
      }

    // It works at INIT only, not at RUNTIME.
      else if ((EStr[i][0] == '+') && (strlen(phoneAut[i]) == 0)) {
    // Checks a used position in EEPROM but enpty in (RAM).
         strcpy(phoneAut[i], EStr[i]); 
    // Copy the string from EStr to phoneAut. Write to phoneAut the number in EStrt.

        // Now the content in RAM(phoneAut)=EEPROM=EStr
      }

    // It works at RUNTIME only, not at INIT
      else if( (EStr[i][0] != '+') && (strlen(phoneAut[i]) > 0)) {
    // An empty position in EEPROM but phone number PRESENT in phoneAut (FLASH)
        writeString((6+i*17), phoneAut[i]);
    // Write to EEPROM (Initial Address 6 and String type data [16 char])
        strcpy(EStr[i], phoneAut[i]);
    // Copy the string from phoneAut to EStr. Write to EStr the number in phoneAut.

    // Now the content in FLASH(phoneAut)=EEPROM=EStr
      }
      
      Serial.print ("EEPROM autorized phone n.");
      Serial.print (i);
      Serial.print (": ");
      Serial.println (EStr[i]);
      
   } 

  Serial.print ("Numero di telefoni: ");
  Serial.println(Auxnphones);
}

void SendMsg()
{
if (gprs.sendSMS(phone, outmessage))
                  { 
                        Serial.print("Send SMS Succeed!\r\n");
		              }
                       else
                    {
                        Serial.print("Send SMS failed!\r\n");
			              }
}       

//   ============  F U N Z I O N I    S T A N D A R D ======================

void setup()
  {
// analogReference(DEFAULT);
    pinMode(PIN_RST, OUTPUT);
    initgsm();
// Inizializza GSM e Valore Tempo iniziale
    EEPROM.update(5, 0);
// Aggiorna EEPROM 5 a 0 solo se non è già a 0 - Presenza rete
    RestorePhones();
// At Power Up o Reset, copia Lista telefoni da EEPROM su phoneAut e n. telefoni (Auxnphones)
  }

void loop()
  {
    /*
      Restart the modem using Software Restart command AT+CFUN=1,1
      or you can hardware restart it.
      AT+CFUN=1,1
    */
    //	  gprs.powerReset(PIN_RESET); //PIN_RESET (7) - Si resetta veramente il Modem GSM?
    //	  init(); // Re-Inizializza - Si res-inizializzazione veramente il Modem GSM?


  unsigned long currentMillis = millis();

  // VERIFICA OGNI 10 secondi (intervalcc) se ci sono SMS da processare o ci sono state variazioni sulla rete elettrica. Gestisce la richieste via SMS di STATUS
  if (currentMillis - previousMilliscc > intervalcc)
   {
      previousMilliscc = currentMillis;     
      // Controlla se ci sono nuovi messaggi SMS
      messageIndex = gprs.isSMSunread();
      if (messageIndex > 0)
          { 
// At least, there is one UNREAD SMS
                gprs.readSMS(messageIndex, message, MESSAGE_LENGTH, phone, datetime);
                Serial.print("At least, there is one UNREAD SMS");
// In order not to full SIM Memory, is better to delete it
                gprs.deleteSMS(messageIndex);
    
                Serial.print("From number: ");
                Serial.println(phone);
                Serial.print("Datetime: ");
                Serial.println(datetime);
                Serial.print("Received Message: ");
                Serial.println(message);

// ============================

// Se messaggio SMS "M" arriva dal numero telefonico autorizzato Master [0] o stringa vuota "" (non definito)
                if (strcmp(phone, phoneAut[0]) == 0 || strlen(phoneAut[0]) == 0)
                    {
// Comando SMS "M+393391255597" (13) "M+393334188263" (13) Sostituisce o Imposta cellulare Autorizzato MASTER 
                        if (message[0] == 'M')
                            {
// Formato Numero errato ?      
                                if ((strlen(message) -2) < 10 || (strlen(message) - 2) > 13)
                                    {
                                        sprintf(outmessage, "%s %s","WRONG TELEPHONE NUMBER FORMAT", message);
                                        Serial.println(outmessage);
                                        SendMsg();
                                    }
                                else
                                      {
// Formato Numero corretto   
                                          strncpy(phoneT, message + 1, strlen(message) - 1);
                                          phoneT[strlen(message) - 1] = '\0';
                                          strcpy(phoneAut[0], phoneT);
                                          writeString(6 + phoneI * 17, phoneT);
// Salva in EEPROM e in phoneAut il nuovo numero telefonico MASTER
                                          sprintf(outmessage, "%s %s","M TELEPHONE NUMBER SAVED", message);
                                          Serial.println(outmessage);
                                          SendMsg(); 
                                      }
                            }             
                    }

// Se messaggio SMS "D" arriva dal numero telefonico autorizzato Master [0]
    if (strcmp(phone, phoneAut[0]) == 0)
        {
          if (message[0] == 'D')
                  {
// Delete in EEPROM and phoneAut auxiliaries phone numbers except the Authorized
// Comando SMS "D" CANCELLA tutti i cellulari eccetto il numero autorizzato (0)   
                    for (int i = 1; i < 3; i++)
                      {
                        phoneAut[i][0] = '\0';
                        writeString((6+i*17), "");  //Initial Address 6 and String type data [16 char])                      
                      }
                    Auxnphones = 0;
                    sprintf(outmessage, "%s","ALL THE AUXILIARY NUMBERS DELETED");
                    Serial.println(outmessage);
                    SendMsg();
                  }
        }

// Se messaggio SMS "E" arriva da qualsiasi telefono - COMANDO NON DOCUMENTATO
if (message[0] == 'E')
        {  
// Delete in EEPROM and phoneAut all phone numbers - COMANDO RISERVATO
            for (int i = 0; i < 3; i++)
                  {
                      phoneAut[i][0] = '\0';
                      writeString((6+i*17), "");  //Initial Address 6 and String type data [16 char])                      
                  }
            Auxnphones = 0;
            sprintf(outmessage, "%s","ALL NUMBERS DELETED");
            Serial.println(outmessage);
            SendMsg();                                   
        }

// ===========================================
// Se messaggio SMS "An" arriva dal numero telefonico autorizzato Master [0]
if (strcmp(phone, phoneAut[0]) == 0)
 { 
// Comando SMS "A1+393391255597" AGGIUNGI/SOSTITUISCI cellulare AUSILIARIO in posizione....               
    if (message[0] == 'A')
          {
// Formato Numero errato ?      
            if ((strlen(message) -3) < 10 || (strlen(message) - 3) > 13)
              {
                sprintf(outmessage, "%s %s","WRONG TELEPHONE NUMBER FORMAT", message);
                Serial.println(outmessage);
                SendMsg();
              } else
                {
// Formato Numero corretto
                  phoneI = atoi(message + 1);  // es. '1' → 1 perchè si ferma al primo carattere non numerico (+)
// Trova l'indice
                  if (phoneI >= 1 && phoneI <= 3)
// Per sicurezza solo numeri ausiliari A1 A2 A3
                  {
                    strncpy(phoneT, message + 2, strlen(message) - 2);
                    phoneT[strlen(message -2)] = '\0';
                    strcpy(phoneAut[phoneI], phoneT);
                    writeString(6 + phoneI * 17, phoneT); // salva in EEPROM

                    sprintf(outmessage, "%s %d %s %s","A", phoneI, "TELEPHONE NUMBER SAVED ", message);
                    Serial.println(outmessage);
                    SendMsg();
                  } else
                        {
                          printf(outmessage, "%s %s","INDEX OUTSIDE THE RANGE", message);
                          Serial.println(outmessage);
                          SendMsg();
                        }
                }
          } // Messaggio A numero valido

  if (message[0] == 'S')
        {
            // Comando SMS "S" Ritorna lo stato della tensione di rete
            // SOLO se il messaggio SMS arriva da un numero autorizzato (Master [0] o Ausiliario[1-3])   

            Auth=0;
            // Inizializza Auth=0 prima dello scan per la verifica 
            // di una richiesta proveniente da un numero autorizzato
            for (int i = 0; i < Auxnphones + 1; i++)
            {
                if (strcmp(phone, phoneAut[i]) == 0)
                {
                  Auth=1;
                  // Il numero richiedente è nella lista dei telefoni (autorizzati)
                  break;
                }
            }
            if (Auth)
             {
                  calc();	//	Calculates PowerVoltage Vrms - Supply voltage
                  Serial.print(" Current Voltage: ");
                  Serial.flush();
                  Serial.println(PowerVoltage);
                  Serial.flush();
                  
                  int power = (int)(PowerVoltage);
                  sprintf(outmessage, "%s %d","CURRENT SUPPLY VOLTAGE: ", power);

                  if (gprs.sendSMS(phone, outmessage))
                      { 
                          Serial.print("Send SMS Succeed!\r\n");
		                  }   else
                          {
                              Serial.print("Send SMS failed!\r\n");
			                    }
            }
            else
                {
// Notifica tentativo non autorizzato
                  strcpy(outmessage, "Request from NOT AUTHORIZED number");
                  Serial.println(outmessage);
                  SendMsg();
                }
        } // Messaggio S
} // Proveniente da numero MASTER
      } // Presenza Messaggi
  } // VERIFICA OGNI 10 secondi (intervalcc)

               
// CalcNphone();
// Calcola/Aggiorna n. telefoni ausiliari (Auxnphones)

// Prevedere la richiesta SMS per vedere quanti e quali numeri sono impostati

// ============ VISUALIZZA STATO SU SERIALE =====================

calc();				//	Calculates PowerVoltage Vrms
Serial.print(" Current Voltage: ");
Serial.flush();
Serial.println(PowerVoltage);
Serial.flush();
	  
if (PowerVoltage <= 180.0)
// Magari considerare anche un limite a 200.0 V per la ripresa 
      {
// =====================   MANCANZA RETE    =====================
// EEPROM.read(5)  0 Rete presente 1 Rete assente
// Identifica transizione da 0 Rete Presente a 1 Rete Assente
        if (EEPROM.read(5) == 0)
            {
              EEPROM.update(5, 1);  // Aggiorna EEPROM a 1
			        int power = (int)(PowerVoltage);
              gprs.getDateTime(locDateTime);
              sprintf(outmessage, "%s %s %d Vac", locDateTime,"MANCANZA RETE, ultima lettura:", power);
			        Serial.println(outmessage);

// Riconoscendo la transizione ON->OFF invia SMS a Numero/i telefono autorizzati
              for (int i = 0; i < Auxnphones + 1; i++)
                    {
                        if (gprs.sendSMS(phoneAut[i], outmessage))
                          { 
                              Serial.print("Send SMS Succeed!\r\n");
		                      } else
                              {
                                  Serial.print("Send SMS failed!\r\n");
			                        }
                    }
		        }
      } // Power Voltage <= 180V

if (PowerVoltage >= 200.0)
// Soglia 200.0 V per la ripresa 
      {
    //                  RETE PRESENTE
    // EEPROM.read(5)  0 Rete presente 1 Rete assente
        if (EEPROM.read(5) == 1)
                {
                    EEPROM.update(5, 0); // Aggiorna EEPROM a 0
                    int power = (int)(PowerVoltage);
                    gprs.getDateTime(locDateTime);
                    sprintf(outmessage, "%s %s %d Vac", locDateTime, "RIPRESA RETE, ultima lettura:", power);
                    Serial.println(outmessage);

// Riconoscendo la transizione OFF->ON invia SMS a Numero/i telefono autorizzati
                    for (int i = 0; i < Auxnphones + 1; i++)
                          {
                              if (gprs.sendSMS(phoneAut[i], outmessage))
                                    { 
                                      Serial.print("Send SMS Succeed!\r\n");
		                                }
                                    else
                                      {
                                        Serial.print("Send SMS failed!\r\n");
			                                } // close the Else

                          } // Close the for 1
                } // Close the EEPROM if (Rete presente)
      } // Close the soglia 200V Presente

/* RESET GIORNALIERO 
Gestisce l'evento di avvenuto reset del GSM controllando giorno e l'ora
(il controllo viene effettuato ogni 15 Minuti) */
if (currentMillis - previousMillisora > intervalora) 
        {
            previousMillisora = currentMillis;
            gprs.getDateTime(locDateTime);
            Serial.print("Verifica ogni 15 Min del RESET Data e Ora: ");
            Serial.println(locDateTime);
            Serial.println("Ora chiama TimeToReset per verificare se è il momento di resettare");
	
            if (TimeToReset() == true)
                {
	                Serial.print (" Devo fare Reset ");
                    initgsm();
                } // close the if 2
      } // close the if 1

  
  } // close the loop function