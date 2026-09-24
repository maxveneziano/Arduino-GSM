#include <Arduino.h>
/* SMS Manager for Power monitoring

  Uses GSM Library GPRS_Shield_Arduino 2024 (Non Suli)

  Specifiche:
  - OK    Verifica corretta registrazione
  - OK    Reinizializzazione ogni 24 ore (ogni giorno) ad ora prestabilita
  - OK    invio SMS mancanza energia fino a 4 numeri
  - OK    invio SMS riattivazione energia fino a 4 numeri
  - OK    Comando SMS (S) per stato Power Supply e SMS a numero richiedente
          + Messaggio SMS per richiedente non autorizzato 
  - (NON Implementato)    Salvataggio stato in memoria non volatile (EEPROM O FLASH)
  - OK da Master    Prevedere la richiesta SMS (CMD N) per vedere quanti e quali numeri sono impostati
          + Messaggio SMS per richiedente non autorizzato 
  - (disabilitazione o abilitazione notifica a TUTTI i numeri da richiesta SMS)
  - OK Cancellazione numeri ausiliari

  - NON Ancora set/reset pin uscita da SMS numero richiedente abilitato
  - NON Ancora Stato pin ingresso su richiesta SMS a numero richiedente abilitato
  - OK Inserimento PIN su comando M (per definizione Master da vuoto) o factory reset da comando M
  - OK  F - Ripristino a condizioni di fabbrica con PIN (solo Master)

  - OK Verifica Indice telefoni ausiliari per evitare sovrapposizioni in input (INDEX OVERLAP) - FATTO
  - OK Prevedere SMS di conferma comandi (richiesta eseguita per il n.) - FATTO

  TODO:
  Gestire più SMS in ricezione (SCANDIRLI TUTTI) e processarli uno alla volta (FIFO) - FATTO
  Magari un ciclo while? FATTO

     .......................... Scopo del codice

Il programma:

- Monitora la tensione di rete tramite un trasformatore e la libreria EmonLib.

- Notifica via SMS a numeri autorizzati (Master + fino a 3 ausiliari) i seguenti eventi:

- Mancanza di rete (tensione < 100V)

- Ripresa rete (tensione > 200V)

- Gestisce comandi via SMS:

- M+numero → Modifica numero Master. Se mai definito, necessita di PIN

- A[n]+numero → Aggiunge/Sostituisce numero ausiliario

- D → Cancella numeri ausiliari

- N → Elenca numeri autorizzati -  OK

- S → Invia stato tensione      -  OK

- E → Comando con PIN riservato, cancella tutti i numeri

- P Cambia PIN / richiede precedente

- F Factory reset - Solo Master con PIN - cancella tutti i numeri (sostituisce E )

- Salva e legge i numeri autorizzati su EEPROM per persistenza.

Numeri GSM

Paese	      Prefisso	Esempio internazionale	Cifre numeriche*	Caratteri con +
🇮🇹 Italia	    +39	    +393391255597	                12	            13
🇩🇪 Germania	+49	    +4915123456789	                13	            14
🇸🇪 Svezia	    +46	    +46701234567	                11	            12
🇫🇮 Finlandia	+358	+358451234567	                  12	            13
🇮🇸 Islanda		+354	+354 611 1234			              10           		11
🇳🇴 Norvegia		+47	+47 912 34 567			             10            		11
🇩🇰 Danimarca	+45	+45 20 12 34 56			              10           		11
🇱🇮 Liechtenstein	+423	+423 661 12 34        			10            		11

  

Comandi SMS
  "M+393391255597" Sostituisci cellulare autorizzato Master con un altro
  "A1+393391255597" AGGIUNGI/SOSTITUISCI cellulare in posizione....
  "D" CANCELLA tutti i cellulari ausiliari (non il Master)

  "N" Elenca tutti i numeri autorizzati - solo per il Master
  "E" Cancella tutti i numeri incluso il Master - NON DOCUMENTATO. Solo con PIN - Accetta qualsiasi numero.

  D → Cancella numeri ausiliari
- N → Elenca numeri autorizzati
- S → Invia stato tensione
- E → Comando con PIN riservato, cancella tutti i numeri
- P Cambia PIN /richiede precedente
- F Factory reset - Solo Master

Struttura EEPROM
PIN (5 Char)                    I -F    Profondità
          6      Inizio PIN     06-11   6 (5 cifre+\0)=6 caratteri
EPINPIN=6 EPPINPRO=6

OLD
n. telefono:
write_String((12+i*16), "");
write_StringTel((EPTELIN+i*EPTELPRO), "", EPTELPRO);

con EPTELIN=12 ed EPTELPRO=16:
        I         I- F      Profondità
0	12+0 12		Inizio MASTER   12-27     16  (14 cifre+"+"+"\0")=16 caratteri
1	12+16 28	Inizio A1	      28-43     16
2	12+32 44 	Inizio A2	      44-59     16
3	12+48 60 	Inizio A3	      60-75     16

  EEPROM.read(5) Indicatore Rete presente (0) Rete assente (1) - NON PIU' UTILIZZATO

  1st Power On
  -  inizialmente la EEPROM non ha numeri (tutti "") 
  -  Definire il numero Master (M)
  -  Una volta definito il numero master è possibile definire il primo numero ausiliario (A1)
     e poi il secondo numero Ausiliario (A2)
  -  Con "D" il MASTER può CANCELLARE tutti i cellulari eccetto il numero Master autorizzato (0)    

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
#define PIN_LED  13
#define BAUDRATE  9600
 /* M. Veneziano 2020 Voltage calibration Transformer + Partition. Set for each specific transformer */ 
#define VOLT_CAL 136.0 
#define MESSAGE_LENGTH 160
#define BUFFER_LENGTH 24
#define EPINPIN 6
#define EPPINPRO 6
#define EPTELIN 12
#define EPTELPRO 16
char message[MESSAGE_LENGTH]; // Message = 160 Char
char outmessage[MESSAGE_LENGTH];
//char outmess[30];
char locDateTime[BUFFER_LENGTH];// Local Date and Time = 25 Char
/* Giorno, Ora e Minuto
Reset Day (rday) a 0 perchè diverso da 1 e da 31 e quindi 
consente il reset fin dalla prima ora giusta del primo giorno */
uint8_t day, rday=0, hh, mm;
//Ora e Minuto di Reset giornaliero
uint8_t ORAr = 03, MINr = 00;
// uint8_t Auxnphones,phoneI;
uint8_t phoneI,lastPhoneIndex;
uint8_t Auth;


//          VARIABILI
//phoneT => phone Temporaneo
//Auxnphones => numero di phones ausiliari
//lastPhoneIndex => indice dell'ultimo telefono occupato.
//phoneI => Index phone (escluso Autorizzato (0))
//Auth => 0= numero non autorizzato 1= numero autorizzato

//epdfl => PIN di Default "123A5"
//PwrActv=true => Default assume rete elettrica presente all'avvio

uint8_t messageIndex = 0;
float PowerVoltage;
char phone [16];
char phoneT [16];
char pinRead [6];
//char pinOld [6];
//char pinNew [6];
char pinDfl [6] = "123A5";
char eprpin [6]; 
bool PwrActv=true; //Default assume rete elettrica presente all'avvio
char datetime[BUFFER_LENGTH];

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
    "+393334188263", // Da rimuovere se passato a terzi
    "",
    "",
    ""
};
// In questo modo ogni cella ha spazio per una stringa di max 15 caratteri più il terminatore \0 =16
// (14 cifre+"+"+\0)=16

// phoneAut[0] authorized master number - Can authorize up to 4 phone numbers - "" used as terminator


// uint32_t iniTime;	// Valore Tempo iniziale
uint32_t previousMilliscc = 0, previousMillisora = 0;
uint32_t intervalcc = 10000; //intervallo per il controllo del valore di tensione attuale - 10 sec
uint32_t intervalora = 900000; //intervallo per il controllo dell'ora - 15 Minuti - 3600000 1 ora
//uint32_t intervalora = 120000; //intervallo per il controllo dell'ora - 2 Minuti - 3600000 1 ora

//COOP INFO SMS (Credito residuo)
#define INFO_NUMBER "4243688"
//#define INFO_NUMBER "3334188263"
#define INFOTXT  "SALDO"
//#define INFOTXT "INFO SIM"

GPRS gprs(PIN_TX, PIN_RX, BAUDRATE); //RX,TX,BaudRate
EnergyMonitor emon1;	//Initialize EnergyMonitor ?

//   ============  F U N Z I O N I ======================
extern int __heap_start, *__brkval;

int freeMemory()
{
    int v;
    return (char *)&v -
           (__brkval == 0
                ? (char *)&__heap_start
                : (char *)__brkval);
}
//  Init GPRS/GSMGPRS Modem with Timeout (30 sec) - Return true if success, false if timeout
bool gprsInitwTO(unsigned long timeoutGSM)
{
    unsigned long startTO = millis();

    while (!gprs.init())
    {
        if (millis() - startTO >= timeoutGSM)
        {
            Serial.println(F("GSM INIT TIMEOUT"));
            return false;
        }

        gprs.powerUpDown(PIN_RST);
        delay(1000);
    }

    return true;
}
//  Wait for Network registration with Timeout (30 sec) - Return true if success, false if timeout

bool waitNetwork(unsigned long timeoutNW)
{
    unsigned long startTO = millis();

    while (!gprs.isNetworkRegistered())
    {
        if (millis() - startTO >= timeoutNW)
        {
            Serial.println(F("NETWORK TIMEOUT"));
            return false;
        }

        delay(1000);
        // Aspetta un secondo e riverifica il collegamento alla rete GSM
    }

    return true;
}


//  Error Stop - Blink LED
  void errorStop()
{
    pinMode(PIN_LED, OUTPUT);

    while (true)
    {
        digitalWrite(PIN_LED, HIGH);
        delay(100);

        digitalWrite(PIN_LED, LOW);
        delay(100);
    }
}

void initapp()
  {
  //Initialize Modem and emoncms

  Serial.begin(9600);

  Serial.print(F("Program started...\nStarting Power On sequence\n"));
  
  emon1.voltage(0, VOLT_CAL, 1.7);  // Defines Voltage: input pin, Voltage calibration, phase_shift
  //emon1.current(0, 32);
  for (uint8_t i = 0; i < 5; i++)
   { //Stabilyze EmonCMS data
    emon1.calcVI(20,2000); // Run 20 measurement made of 20 halfwave (200ms) with a 2000ms Timeout
   }

  if (!gprsInitwTO(30000)) // 30 sec timeout
{
    Serial.println(F("ERRORE: GSM INIT TIMEOUT"));
    errorStop();
    // Blocca l'esecuzione e notifica con un led lampeggiante
}
// delay(1000); Da rimuovere ?
 
  Serial.print(F(" - Init Success - Completed GSM Power On Sequence - Reset\n"));
  // iniTime = millis(); // Valore Tempo iniziale
  
// Garantisce che il Modem sia registrato sulla rete entro 60 sec
if (!waitNetwork(60000))
{
    Serial.println(F("Rete GSM non disponibile"));
    errorStop();
    // Blocca l'esecuzione e notifica con un led ad esempio lampeggiante
}
  Serial.print(F("GSM network initialization done!\n"));

// ###########################   IMPOSTAZIONI SMS

// SELEZIONA MEMORIA SMS SIM CARD
sim900_check_with_cmd(F("AT+CPMS=\"SM\",\"SM\",\"SM\"\r\n"), "OK", CMD);
delay(500);

//AT+CMGF=1	// Enable ASCII TEXT mode for SMS
  if (sim900_check_with_cmd(F("AT+CMGF=1\r\n"), "OK", CMD))
    {
// Set message mode to ASCII
      Serial.println (F (" Set ASCII TEXT mode for SMS...."));
    }
   delay(500);  

  // ###########################   PREVENTIVELY DELETE ALL SMS UNREAD
  sim900_check_with_cmd(F("AT+CMGD=1,4\r\n"), "OK", CMD);
  delay(5000);   

  // Invia SMS al numero Coop Voce 42 43 688 INFO SIM per credito residuo
  // in modo da ricavare la data e l'ora corrente
  Serial.print(F("Invio Messaggio INFO\n"));

  // Send SMS to defined phone number and text
  if (gprs.sendSMS(INFO_NUMBER, INFOTXT))
    { 
        Serial.print(F("Send SMS Succeed!\r\n"));
        Serial.flush();
    } 
  else
    {
      Serial.print(F("Send SMS failed!\r\n"));
      Serial.flush();
    }


  // #################  Legge il messaggio INFO ricevuto ################
  // C'è il rischio che si frapponga un SMS di servizio del provider
  // Solo se non passa molto tempo dalla registrazione alla rete
  // (Vedi cancellazione preventiva)
    Serial.println(F("Attende la ricezione del messaggio INFO"));

    // Attende di ricevere il messaggio INFO
    while (messageIndex < 1 || messageIndex == 255)
    {   
      if (messageIndex == 255)
        { 
          Serial.print (F ("Waiting INFO Message 255 code Error!\r\n"));
          errorStop();
        }
        else

          {
          //delay(500);  
          // Si prepara per il prossimo ciclo
             messageIndex = gprs.isSMSunread();
                
             Serial.print(F("No SMS received yet!\n"));

             Serial.print(F("Waiting for INFO SMS - New messageIndex: "));
             Serial.println(messageIndex);
           }
    }
// Messaggio Ricevuto - messageIndex >= 1
      // delay(100); Da cancellare?
      //Serial.flush();

      Serial.print(F("SMS received - Current messageIndex: "));
      Serial.println(messageIndex);

      //sim900_flush_serial();
      //delay(5000);


  // Legge in continuazione SMS finchè non rimangono più messaggi non letti (messageIndex=0)
  // L'SMS di INFO è probabilmente il più recente e quindi aggiorna con i dati dell'ultimo SMS ricevuto.
  // Riconoscendo il codice 255 si blocca facendo lampeggiare il led in quanto si tratta di
  // un probabile errore di comunicazione con il modem, modem non inizializzato
  // o non collegato alla rete
  // ###########################################################################
  
  //messageIndex = gprs.isSMSunread();
  
  //while ((messageIndex = gprs.isSMSunread()))
  while ((messageIndex = gprs.isSMSunread()) != 0)
    {

    if (messageIndex == 255)
        { 
          Serial.print (F ("SMS INFO READ 255 code Error!\r\n"));
          errorStop();
        }
    if (gprs.readSMS(messageIndex, message, MESSAGE_LENGTH, phone, datetime))
        {
        delay(1000);

        Serial.print(F("SMS indice: "));
        Serial.println(messageIndex);

        Serial.print(F("Da: "));
        Serial.println(phone);

        Serial.print(F("Testo: "));
        Serial.println(message);

        gprs.deleteSMS(messageIndex);
        }
 
}

// ######################  CANCELLA TUTTI GLI SMS
    //In order to not full SIM Memory, is better to delete all SMS

  Serial.print(F("CANCELLA TUTTI GLI SMS: "));

  sim900_check_with_cmd(F("AT+CMGD=1,4\r\n"), "OK", CMD);
  delay(5000);

    messageIndex = gprs.isSMSunread();
    // delay(2000);

    Serial.print(F("messageIndex - After ALL SMS deletion: "));
    Serial.println(messageIndex);

    Serial.println(F("RIASSUNTO: "));
    Serial.println(F("From number: "));
    Serial.println(phone);
    //Serial.flush();
    Serial.println(F("Datetime: "));
    Serial.println(datetime);
    Serial.flush();
    Serial.println(F("Received Message:"));
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
    
if (sim900_check_with_cmd (outmessage, "OK", CMD))
      {
        Serial.println (F("A buon Fine"));
        Serial.println (outmessage);
        Serial.flush();
      }
      else
        {
            Serial.println (F ("Non a buon Fine"));
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

}

//   ============  A L T R E   F U N Z I O N I ======================

void SendMsg()
{
if (gprs.sendSMS(phone, outmessage))
                  { 
                        Serial.print(F("Send SMS Succeed!\r\n"));
		              }
                       else
                    {
                        Serial.print(F ("Send SMS failed!\r\n"));
			              }
}

void write_String(uint8_t offs, const char *edata, uint8_t maxLen)
{
    uint8_t i = 0;

    while (edata[i] != '\0' && i < maxLen - 1)
    {
        EEPROM.update(offs + i, edata[i]);
        i++;
    }

    EEPROM.update(offs + i, '\0'); // Aggiunge terminatore
}

void read_String(uint8_t offs, char *dest, uint8_t maxLen)
{
    uint8_t i;

    for (i = 0; i < maxLen - 1; i++)
    {
        dest[i] = EEPROM.read(offs + i);

        if (dest[i] == '\0')
            break;
    }

    dest[i] = '\0';
}

bool TimeToReset () {
// Reset Now?

  //  Indice parte da 0
//    locDateTime - String like "24/05/29,10:30:15+08" "yy/MM/dd,hh:mm:ss+/-zz"
//                               0123456789
//                              Day = 6,7
//                              hh = 9,10
//                              mm = 12,13


  // day = (locDateTime[0] - '0') * 10 + (locDateTime[1] - '0');
  day = (locDateTime[6] - '0') * 10 + (locDateTime[7] - '0');
  hh = (locDateTime[9] - '0') * 10 + (locDateTime[10] - '0');
  mm = (locDateTime[12] - '0') * 10 + (locDateTime[13] - '0');

  Serial.print (F("ORA, MIN --> "));
  Serial.print(hh);
  Serial.print(F(":"));
  Serial.print(mm);
  Serial.println (F(" <--"));
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

void CalcAuxnphones()
 {
  // Loop - Calculates number of auxiliary phones (lastPhoneIndex)
  // Remember that in phoneAut if the first char of an entry (phone) is '+'
  // it is considered that a phone is loaded
  // Con "break" esce dal loop con "i" che ha contato l'indice (che parte da 0)
  // di quante stringhe di telefoni ausiliari c'erano.
  // ATTENZIONE ! Nel caso di lastPhoneIndex=0 
  // vale anche in presenza/assenza del numero Master
  // Quindi in realtà indica il numero dei telefoni An ausiliari
  /*
  phoneAut[0]=Master
  phoneAut[1]=A1
  phoneAut[2]=A2
  phoneAut[3]=A3
  */
 lastPhoneIndex = 0; // Inizializza preventivamente a 0

 for (uint8_t i = 0; i < 4; i++)
    {
        if (phoneAut[i][0] == '+')
// Checks the presence of a Phone number in phoneAut. The "+" character.
// If found, sets lastPhoneIndex to i. If not, breaks the loop and exit
            {
                lastPhoneIndex = i;
// Counts valid phones
            } 
        else 
            {
              // No valid phone found: stop scanning.
                break;
            }
      }
  }

void ListAutPhones()
{
    lastPhoneIndex = 0;

    // Inizializza il messaggio
    outmessage[0] = '\0';

    // Numero di caratteri già presenti in outmessage
    size_t len = 0;

    for (uint8_t i = 0; i < 4; i++)
    {
        if (phoneAut[i][0] == '+')
        {
            lastPhoneIndex = i;

            // Aggiunge direttamente la nuova riga a outmessage
            len += snprintf_P(
                outmessage + len,
                sizeof(outmessage) - len,
                PSTR("Authorized phone n. %d: %s\n"),
                i,
                phoneAut[i]
            );

            // Scrive su Serial
            Serial.print(F("Authorized phone n."));
            Serial.print(i);
            Serial.print(F(": "));
            Serial.println(phoneAut[i]);
        }
        else
        {
            break;
        }
    }

    Serial.print(F("Numero di telefoni ausiliari + Master: "));
    Serial.println(lastPhoneIndex + 1);

    SendMsg();
}


/*
void ListAutPhones()
 {
  // Remember that in phoneAut if the first char of an entry (phone) is '+'
  // it is considered that a phone is loaded

  // Auxiliary numbers are set only if a MASTER number is present (phoneAut[0] = Master number)
  // Because it is called from the CMD N, it is sure that the MASTER number should be is present 
  /* 
    Elenca e stampa i telefoni autorizzati.

    Crea un SMS con tutti i numeri utilizzando le variabili globali:
    - phone
    - outmessage

    Aggiorna lastPhoneIndex.

    Invia l’elenco (outmessage) tramite SMS al numero (phone) che ha richiesto l’informazione (solo il Master). */

// °°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°
/*
    lastPhoneIndex = 0;
    outmessage [0] = '\0';
    // outmess [0] = '\0';
    for (uint8_t i = 0; i < 4; i++)
    {
        if (phoneAut[i][0] == '+')
// Checks the presence of a Phone number (*) in EEPROM. If not breaks the loop with lastPhoneIndex set i
            {
                lastPhoneIndex = i;
// Counts valid phones
            } 
        else 
            {
                break;
// Empty position
            }

  //sprintf(outmessage, "Authorized phone n. %d: %s\n", i, phoneAut[i]);
  sprintf(outmess, "Authorized phone n. %d: %s\n", i, phoneAut[i]);
  strcat(outmessage, outmess);

  // Scrive su Serial
  Serial.print (F("Autorized phone n."));
  Serial.print (i);
  Serial.print (F (": "));
  Serial.println (phoneAut[i]);
    }

Serial.print (F ("Numero di telefoni ausiliari + Master: "));
Serial.println (lastPhoneIndex+1);
SendMsg();

}

*/

  void RestorePhones()
 {
  // At INIT (Power ON) copy EEPROM to phoneAut

  // If in EEPROM the first char of an entry (phone) is '+' it is considered that a phone is loaded
  // Otherwise load the predefined number as defined in RAM (phoneAut).

  // Scan and Read from EEPROM content (authorized phone numbers, Master included) and copy the content to phoneAut 
  // Remember that auxiliary numbers are set only if a MASTER number is present (phoneAut[0] = Master number)
    lastPhoneIndex = 0;
    for (uint8_t i = 0; i < 4; i++)
    {
        //OLD read_StringTel(6 + i*16, phoneT);
        read_String(EPTELIN + i*EPTELPRO, phoneT, EPTELPRO);
        // Nuova funzione che legge la EEPROM e copia in phoneT (phoneAut) - elemento "i"
        
        // Checks the presence of a Phone number (*) in EEPROM.
        // if (phoneAut[i][0] == '+')
          if (phoneT[0] == '+')

        {
            // If number is present, breaks the loop and set lastPhoneIndex to i
            strcpy(phoneAut[i], phoneT);
            lastPhoneIndex = i;
        } 
        else 
        {
        // If no number present, breaks the loop
        break;
        // Empty position
        }
    // Now the content in RAM (phoneAut) = EEPROM
    // Except for the first time when EEPROM is empty and phoneAut has a predefined number
    // Counts valid phones
    Serial.print (F ("Autorized phone n."));
    Serial.print (i);
    Serial.print (F (": "));
    Serial.println (phoneAut[i]);

   } 
  Serial.print (F ("Numero di telefoni ausiliari + Master: "));
  Serial.println(lastPhoneIndex+1);
}


//   ============  FUNZIONI    STANDARD ======================

void setup()
  {
  // Poichè viene eseguito al Power Up (prima alimentazione o disalimentazione rete o batteria),
  // si suppone che sia presente la rete.
    pinMode(PIN_RST, OUTPUT);
    
// Inizializza GSM e Valore Tempo iniziale - Per evitare valori non determinati
    initapp();

    Serial.print(F("RAM libera dopo initapp(): "));
    Serial.println(freeMemory());

// EEPROM.update(5, 0);
// Aggiorna EEPROM 5 a 0 solo se non è già a 0 - Presenza rete

    PwrActv=true; // Assume rete presente al primo avvio

    RestorePhones();
// At Power Up copia Lista telefoni da EEPROM su phoneAut e n. telefoni (lastPhoneIndex)

pinDfl [05] = '\0'; // Aggiunge Terminatore stringa

read_String(EPINPIN, eprpin, EPPINPRO);
// At Power Up legge il PIN in EEPROM


if (strlen(eprpin) != 5)
// Se EEPROM non contiene un PIN di 5 caratteri (Sporca)
    {
      // Se EEPROM sporca o vuota, scrive il PIN di default "123A5"
      write_String(EPINPIN, pinDfl, EPPINPRO);
      Serial.print(F("Executing Setup "));
      Serial.print(F("!= 5 eprpin " ));
      Serial.println(eprpin);
      Serial.println (strlen(eprpin));
      strcpy(eprpin, pinDfl);      // scrive in eprpin il PIN di default "123A5"
    }

      Serial.print(F("Executing Setup "));
      Serial.print(F("== 5 eprpin " ));
      Serial.println(eprpin);
      Serial.println (strlen(eprpin));

  }

void loop()
  {
   
  unsigned long currentMillis = millis();

  // VERIFICA OGNI 10 secondi (intervalcc) se ci sono SMS da processare
  // o ci sono state variazioni sulla rete elettrica.
  // Gestisce la richieste via SMS
  if (currentMillis - previousMilliscc > intervalcc)
   {
      previousMilliscc = currentMillis;  
      
      // Attende la ricezione di almeno un messaggio
      messageIndex = gprs.isSMSunread();
      // delay(500); 

// ########################################################################################      
//                         INIZIO PARSER COMANDI SMS
// ######################################################################################## 

    while (messageIndex > 0 && messageIndex != 255)
    // Considera anche il caso di errore =255
    // In caso di errore NON esegue il Parser ed esce dal While
    // Fuori dal While indica l'errore sulla serial output
    // e blocca l'esecuzione lampeggiando il led
    {   
      Serial.print(F("SMS received !\n"));
      Serial.print(F("Received one SMS - messageIndex: "));
      Serial.println(messageIndex);

// At least, there is one UNREAD SMS then reads the content of the SMS
// and deletes it from SIM memory to avoid filling it up
                gprs.readSMS(messageIndex, message, MESSAGE_LENGTH, phone, datetime);
                delay (1000);
                Serial.print (F("At least, there is one UNREAD SMS"));
// In order not to full SIM Memory, is better to delete it
                gprs.deleteSMS(messageIndex);
                // Si prepara per il prossimo ciclo di lettura SMS
                messageIndex = gprs.isSMSunread();
                //delay(500);

// Write on Serial Monitor the content of received SMS.
                Serial.print (F ("From number: "));
                Serial.println(phone);
                Serial.print (F ("Datetime: "));
                Serial.println(datetime);
                Serial.print (F ("Received Message: "));
                Serial.println(message);

// ############################## COMANDO M - Sostituisce o Imposta cellulare Autorizzato MASTER
// ====================================================================================
// Se messaggio SMS arriva dal numero telefonico autorizzato MASTER [0]
// o stringa vuota "" (Factory - nessun telefono ancora registrato -> richiede il PIN)
// Telefono Master già inserito                         Regime -  Comando: M+393391255597
// Telefono Master non ancora inserito (EEPROM vuota) - Inizio -  Comando: M+393391255597 12345
// ====================================================================================
                
// Comando SMS "M+393391255597" (12) "M+393334188263" (12)
                        if (message[0] == 'M')
                          {
                            if (strlen(phoneAut[0]) == 0)
                              // Telefono Master vuoto -> richiede comando con PIN
                            {
                              // Lunghezza Numero errato ? 16 > N > 19   compreso tra 16 e 19  
                                  if ((strlen(message) -2) < 16 || (strlen(message) - 2) > 19)
                                    { 
                                        sprintf(outmessage, "WRONG M NUMBER OR PIN FORMAT %s", message);
                                        Serial.println(outmessage);
                                        SendMsg();
                                    }
                                else // Formato (Numero e PIN) corretto
                                      {
                                        // Restituisce il n.telefonico comprensivo di "+" e "/0")
                                        int i = 0;
                                        while (message[i + 1] != ' ' && message[i + 1] != '\0')
                                            {
                                                phoneT[i] = message[i + 1];
                                                i++;
                                            }
                                        phoneT[i] = '\0';

                                        // Estrae il PIN Restituisce il PIN comprensivo "/0")
                                        int j = 0;
                                        while (message[i + 2 + j] != '\0' && j < 5)
                                            {
                                                pinRead[j] = message[i + 2 + j];
                                                j++;
                                            }
                                        pinRead[j] = '\0';

                              // PIN valido solo se ha esattamente 5 caratteri E il messaggio finisce qui
                                        if (j != 5 || message[i + 2 + j] != '\0')
                                        {
                                            sprintf(outmessage, "WRONG PIN FORMAT %s", message);
                                            Serial.println(outmessage);
                                            SendMsg();
                                        }
                                            else
                                            {
                                            // Formato (PIN) corretto
                                            // Salva in EEPROM e in phoneAut il nuovo numero telefonico MASTER  
                                                                                    
                                            if (strcmp(eprpin, pinRead) != 0)
                                                {
                                                 sprintf(outmessage, "WRONG PIN %s", message);
                                                 Serial.println(outmessage);
                                                 SendMsg();
                                                }
                                                else
                                                  // PIN corretto
                                                    {
                                                    strcpy(phoneAut[0], phoneT);
                                                    write_String(EPTELIN, phoneT,EPTELPRO);

                                                    sprintf(outmessage, "M TELEPHONE NUMBER + PIN SAVED %s", phoneT);
                                                    Serial.println(outmessage);
                                                    SendMsg();
                                                    }
                                              } //Salva in EEPROM e in phoneAut il nuovo numero telefonico MASTER
                                            } // End Formato (Numero e PIN) corretto
                                } // End Numero M Vuoto
                                
                                else if (strcmp(phone, phoneAut[0]) == 0)
                                      {
                                  // Lunghezza Numero errato ? 10 > N > 13   compreso tra 10 e 13   
                                        if ((strlen(message) - 2) < 10 || (strlen(message) - 2) > 13)
                                          {
                                            sprintf(outmessage, "WRONG M NUMBER FORMAT %s", message);
                                            Serial.println(outmessage);
                                            SendMsg();
                                          }
                                        else 
                                            {
                                              // Formato Numero M corretto
                                              // Salva in EEPROM e in phoneAut il nuovo numero telefonico MASTER  
                                            
                                              strncpy(phoneT, message + 1, strlen(message) - 1);
                                              phoneT[strlen(message) - 1] = '\0';
                                        
                                            
                                              strcpy(phoneAut[0], phoneT);
                                              write_String(EPTELIN, phoneT, EPTELPRO);

                                              sprintf(outmessage, "M TELEPHONE NUMBER SAVED %s", phoneT);
                                              Serial.println(outmessage);
                                              SendMsg();
                                            }
                                            } // FINE Numero MASTER in EEPROM
                                            
                                     else
                                      {
                                      // Notifica tentativo non autorizzato
                                      strcpy(outmessage, "Request from NOT AUTHORIZED number");
                                      Serial.println(outmessage);
                                      SendMsg();
                                      } 
                                
                            } // FINE messaggio "M"

// ################################# COMANDO D DELETE AUXILIARIES
// Comando SMS "D" cancella tutti i numeri eccetto il MASTER
// SOLO se il messaggio SMS di richiesta è proveniente dal MASTER[0]
//Se messaggio SMS è di tipo "D"
            if (message[0] == 'D')
            //if (strcmp(message, "D") == 0)
                {
                    if (strcmp(phone, phoneAut[0]) == 0) // Proveniente da telefono registrato MASTER
                        {
//Delete in EEPROM and phoneAut auxiliaries phone numbers except the Authorized MASTER[0]
                          for (uint8_t i = 1; i < 4; i++)
                            {
                              phoneAut[i][0] = '\0';
                              write_String((EPTELIN+i*EPTELPRO), "", EPTELPRO);
                              // OLD Initial Address 6 and String type data [16 char])
                              // OLD write_String((6+i*16), "");                        
                            }
                          lastPhoneIndex = 0;
                      // sprintf(outmessage, "ALL THE AUXILIARY NUMBERS ARE DELETED");
                      strcpy(outmessage, "ALL THE AUXILIARY NUMBERS ARE DELETED");
                      Serial.println(outmessage);
                      SendMsg();                    
                        }
                        else
                        {
// Notifica tentativo non autorizzato
                            strcpy(outmessage, "Request from NOT AUTHORIZED number");
                            Serial.println(outmessage);
                            SendMsg();
                        }
                } // ############################### Fine comando D

// ########################################### Comando A
// Comando SMS "A1+393391255597" AGGIUNGI/SOSTITUISCI cellulare AUSILIARIO in posizione.... 
// Valido solo se messaggio SMS "An" arriva dal numero telefonico autorizzato Master [0]
              
if (message[0] == 'A')
{
    if (strcmp(phone, phoneAut[0]) == 0) // Solo se numero Master
    {      
// Formato Numero errato ?
// Lunghezza Numero errato  10 > N > 13   compreso tra 10 e 13       
            if ((strlen(message) -3) < 10 || (strlen(message) - 3) > 13)
              {
                sprintf(outmessage, "WRONG TELEPHONE NUMBER FORMAT %s", message);
                Serial.println(outmessage);
                SendMsg();
              } else
                {
// Formato Numero corretto
                  phoneI = atoi(message + 1);  // es. '2' → 2 perchè si ferma al primo carattere non numerico (+)
// Trova l'indice
                  if (phoneI >= 1 && phoneI <= 3)
// Per sicurezza solo nel caso phoneI sia compreso tra 1 e 3 (A1 A2 A3)
                    {
                      CalcAuxnphones(); // Calcola il numero attuale di telefoni ausiliari (lastPhoneIndex)
                      
                      if (phoneI <= lastPhoneIndex+1)
                      // Se l'indice è già presente o è il prossimo da sostituire
                      // Se è il primo inserimento di A1 (lastPhoneIndex)=0
                            {
                                strncpy(phoneT, message + 2, strlen(message) - 2);
                                phoneT[strlen(message) - 2] = '\0';
                                strcpy(phoneAut[phoneI], phoneT); // Salva in FLASH
                                // OLD write_String(6 + phoneI * 16, phoneT);
                                write_String(EPTELIN + phoneI * EPTELPRO, phoneT, EPTELPRO);                               // Salva EEPROM

                                CalcAuxnphones(); // Aggiorna il numero di telefoni ausiliari (lastPhoneIndex)

                                sprintf(outmessage, "%s%d %s %s","A", phoneI, " TELEPHONE NUMBER SAVED ", message);
                                Serial.println(outmessage);
                                SendMsg();
                            } else
                              {
                                    sprintf(outmessage, "WRONG - INDEX OVERLAP %s", message);
                                    Serial.println(outmessage);
                                    SendMsg();
                              }                              
                  } else
                        {
                          sprintf(outmessage, "INDEX OUTSIDE THE RANGE %s", message);
                          Serial.println(outmessage);
                          SendMsg();
                        }
                }
    }
    else
    {
        strcpy(outmessage, "Request from NOT AUTHORIZED number");
        Serial.println(outmessage);
        SendMsg();
    }
}
// ########################################### Fine - Comando A

// ########################################### Comando P
// Comando SMS "P 12345 54321" SOSTITUISCE PIN (PIN OLD, PIN NEW)
// Valido solo se arriva dal numero telefonico autorizzato Master [0]

if (message[0] == 'P')
{
    if (strcmp(phone, phoneAut[0]) == 0)
    {
        // Formato corretto:
        // "P 12345 54321" = 13 caratteri
        if (strlen(message) != 13)
        {
            strcpy(outmessage, "WRONG PINs FORMAT ");
            strcat(outmessage, message);

            Serial.println(outmessage);
            SendMsg();
        }
        else
        {
            // Confronta direttamente il vecchio PIN:
            // message[2..6] con eprpin
            if (strncmp(message + 2, eprpin, 5) == 0)
            {
                // Il vecchio PIN è corretto.
                // Copia direttamente il nuovo PIN:
                // message[8..12] -> eprpin

                strncpy(eprpin, message + 8, 5);
                eprpin[5] = '\0';

                // Salva il nuovo PIN in EEPROM
                write_String(EPINPIN, eprpin, EPPINPRO);

                strcpy(outmessage, "PIN SAVED");

                Serial.println(outmessage);
                SendMsg();
            }
            else
            {
                // Vecchio PIN errato
                strcpy(outmessage, "WRONG OLD PIN ");
                strcat(outmessage, message);

                Serial.println(outmessage);
                SendMsg();
            }
        }
    }
    else
    {
        // Numero non autorizzato
        strcpy(outmessage, "Request from NOT AUTHORIZED number");

        Serial.println(outmessage);
        SendMsg();
    }
}
// ########################################### Fine Comando P




// Comando SMS "P 12345 54321" SOSTITUISCI PIN (PIN OLD, PIN NEW)
// Valido solo se messaggio SMS "P 12345 54321" arriva dal numero telefonico autorizzato Master [0]
              
/*
if (message[0] == 'P')
{  
    if (strcmp(phone, phoneAut[0]) == 0)
    { // Solo se arriva dal numero telefonico autorizzato Master [0]
      // Formato PIN errato ?      
      if (strlen(message) != 13)
        {
          // Formato PIN errato
          sprintf(outmessage, "WRONG PINs FORMAT %s", message);
          Serial.println(outmessage);
          SendMsg();
        } else
          {
            strncpy(pinOld, message + 2, 5);
            pinOld[5] = '\0';

            strncpy(pinNew, message + 8, 5);
            pinNew[5] = '\0';

            if (strcmp(pinOld, eprpin) == 0)
            {
            // Formato PIN corretto
            strcpy(eprpin, pinNew); // Salva in RAM
            write_String(EPINPIN, eprpin, EPPINPRO); // Salva in EEPROM

            sprintf(outmessage, "PIN SAVED");
            Serial.println(outmessage);
            SendMsg();
            } 
            else
              {
                sprintf(outmessage, "WRONG OLD PIN %s", message);
                Serial.println(outmessage);
                SendMsg();
              }
            }

    }
    else
    {
        strcpy(outmessage, "Request from NOT AUTHORIZED number");
        Serial.println(outmessage);
        SendMsg();
    } 
}
// ########################################### Fine - Comando P
*/

// ########################################### Comando N
// Valido solo se la richiesta proviene dal MASTER

//Se messaggio SMS è di tipo "N"
            if (message[0] == 'N')
                {
// Comando SMS "N" Ritorna i numeri autorizzati
// SOLO se il messaggio SMS e la richiesta è proveniente dal numero autorizzato MASTER[0]

                    if (strcmp(phone, phoneAut[0]) == 0)
                        {
                            ListAutPhones();                    
                        }
                    else
                        {
// Notifica tentativo non autorizzato
                            strcpy(outmessage, "Request from NOT AUTHORIZED number");
                            Serial.println(outmessage);
                            SendMsg();
                        }
                }
// ################################### Fine - Comando N

// ########################################### Comando F    
// ##################
// ################## Se messaggio SMS "F" arriva da Telefono MASTER + PIN Comando: 12345

// Comando SMS "F12345" (6)
                        if (message[0] == 'F')
                            { 
                              if (strcmp(phone, phoneAut[0]) == 0)
                                {                              
                              // Lunghezza Comando errato ? 5 
                                  if ((strlen(message) -1) != 5)
                                    {
                                        sprintf(outmessage, "WRONG PIN FORMAT %s", message);
                                        Serial.println(outmessage);
                                        SendMsg();
                                    }
                                   else // Formato PIN corretto
                                      {
                                        // Estrae il PIN Restituisce il PIN comprensivo con "/0")
                                        int i = 0;
                                        // while (message[i + 1 + j] != '\0')
                                        while (message[i + 1] != '\0' && i < 5)
                                            {
                                                pinRead[i] = message[i + 1];
                                                i++;
                                            }
                                        pinRead[i] = '\0';
                                          
                                            // Formato (PIN) corretto
                                                                                                                                
                                            // Controllo equivalenza PIN
                                            if (strcmp(eprpin, pinRead) != 0)
                                                {
                                                 sprintf(outmessage, "WRONG PIN %s", message);
                                                 Serial.println(outmessage);
                                                 SendMsg();
                                                }
                                                else
                                                {
                                                  // PIN corretto

                                                  // Delete in EEPROM and phoneAut (FLASH) all phone numbers - Set PIN to Factory 123A5
                                                  for (uint8_t i = 0; i < 4; i++)
                                                    {
                                                      phoneAut[i][0] = '\0';
                                                    // OLD write_String((6+i*16), "");
                                                    // OLD Initial Address 6 and String type data [16 char])
                                                      write_String((EPTELIN+i*EPTELPRO), "", EPTELPRO);                      
                                                    }
                                                  write_String(EPINPIN, pinDfl, EPPINPRO);
                                                  strcpy(eprpin, pinDfl);      // scrive in eprpin il PIN di default "123A5" 
                                                  lastPhoneIndex = 0;
                                                  sprintf(outmessage, "ALL NUMBERS DELETED & PIN SET TO FACTORY");
                                                  Serial.println(outmessage);
                                                  SendMsg();
                                                }                        
                                                  
                                              } // End Formato PIN corretto 
                            } // FINE messaggo proveniente da MASTER
                            else
                              {
                                strcpy(outmessage, "Request from NOT AUTHORIZED number");
                                Serial.println(outmessage);
                                SendMsg();
                              }

                          } //  FINE comando F

// #################################### Fine Comando F

// #################################### Comando S
// Valido solo se proveniente da numero Autorizzato (Master o  Ausiliario)
    if (message[0] == 'S')        {
            // Comando SMS "S" Ritorna lo stato della tensione di rete
            // SOLO se il messaggio SMS arriva da un numero autorizzato (Master [0] o Ausiliario[1-3])   

            Auth=0;
            // Inizializza Auth=0 prima dello scan per la verifica 
            // di una richiesta proveniente da un numero autorizzato
            for (uint8_t i = 0; i < lastPhoneIndex + 1; i++)
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
                  
                  // Prepares for the SMS and writes on Serial Monitor
                  int power = (int)(PowerVoltage);
                  gprs.getDateTime(locDateTime);
                  sprintf(outmessage, "%s Current Voltage: %d Vac", locDateTime, power);
                  Serial.println(outmessage);    // Writes on Serial Monitor the current voltage and the date/time       

                  if (gprs.sendSMS(phone, outmessage)) // Sends the SMS to the requesting number and check the result
                      { 
                          Serial.print (F ("Send SMS Succeed!\r\n"));
		                  }   else
                          {
                              Serial.print (F ("Send SMS failed!\r\n"));
			                    }
            }
            else
                {
// Notifica tentativo non autorizzato
                  strcpy(outmessage, "Request from NOT AUTHORIZED number");
                  Serial.println(outmessage);
                  SendMsg();
                }
        }
// #################################### Fine Comando S

    } // ####################################  FINE CICLO WHILE PROCESSAMENTO COMANDI SMS
// Finiti SMS da processare, esce dal ciclo while con MessageIndex anche in caso di errore (255)
// Poi continua con il loop principale

// Se messageIndex = 255, significa che il modem non risponde più e va resettato
     if (messageIndex == 255)
                      { 
                          Serial.print (F ("PARSER code 255 Error!\r\n"));
                          errorStop();
                          // Blocca l'esecuzione e notifica con un led ad esempio lampeggiante
		                  } 
  }
// FINE VERIFICA OGNI 10 secondi (intervalcc)



// ============ VISUALIZZA STATO SU SERIALE IN MODO CONTINUO =====================

calc();				//	Calculates PowerVoltage Vrms
Serial.print (F (" Current Voltage: "));
Serial.flush();
Serial.println(PowerVoltage);
Serial.flush();

  gprs.getDateTime(locDateTime); 
  day = (locDateTime[6] - '0') * 10 + (locDateTime[7] - '0');
  hh = (locDateTime[9] - '0') * 10 + (locDateTime[10] - '0');
  mm = (locDateTime[12] - '0') * 10 + (locDateTime[13] - '0');

  Serial.print (F("ORA, MIN --> "));
  Serial.print(hh);
  Serial.print(F(":"));
  Serial.print(mm);
  Serial.println (F(" <--"));
	  
if (PowerVoltage <= 100.0)
      {
// =====================   MANCANZA RETE    =====================

// EEPROM.read(5)  0 Rete presente 1 Rete assente
// Identifica transizione da 0 Rete Presente a 1 Rete Assente
/*       if (EEPROM.read(5) == 0)
            {
              EEPROM.update(5, 1); Aggiorna EEPROM a 1 */

        if (PwrActv == true) // significa che la rete era presente e quindi invia SMS di notifica
			        {
              PwrActv = false; // Resetta la variabile di stato rete presente  
              int power = (int)(PowerVoltage);
              gprs.getDateTime(locDateTime);
//sprintf(outmessage, "%s %s %d Vac", locDateTime," MANCANZA RETE, ultima lettura:", power);
              sprintf(outmessage, "%s MANCANZA RETE, ultima lettura: %d Vac", locDateTime, power);
            
              Serial.println(outmessage);

// Riconoscendo la transizione ON->OFF invia SMS a Numero/i telefono autorizzati
              for (uint8_t i = 0; i < lastPhoneIndex + 1; i++)
                    {
                        if (gprs.sendSMS(phoneAut[i], outmessage))
                          { 
                              Serial.print (F ("Send SMS Succeed!\r\n"));
		                      } else
                              {
                                  Serial.print (F ("Send SMS failed!\r\n"));
			                        }
                    }
		        }
      } // Power Voltage <= 100V

if (PowerVoltage >= 200.0)
// Soglia 200.0 V per la ripresa 
      {
    //                  RETE PRESENTE

    /* EEPROM.read(5)  0 Rete presente 1 Rete assente
        if (EEPROM.read(5) == 1)
                {
                    EEPROM.update(5, 0); // Aggiorna EEPROM a 0 */

                if (PwrActv == false) // significa che la rete era presente e quindi invia SMS
                {
                    PwrActv = true; // Resetta la variabile di stato rete presente  
                    int power = (int)(PowerVoltage);
                    gprs.getDateTime(locDateTime);
//                  sprintf(outmessage, "%s %s %d Vac", locDateTime, "RIPRESA RETE, ultima lettura:", power);
                    sprintf(outmessage, "%s RIPRESA RETE, ultima lettura: %d Vac", locDateTime, power);
                    
                    Serial.println(outmessage);

// Riconoscendo la transizione OFF->ON invia SMS a Numero/i telefono autorizzati
                    for (uint8_t i = 0; i < lastPhoneIndex + 1; i++)
                          {
                              if (gprs.sendSMS(phoneAut[i], outmessage))
                                    { 
                                      Serial.print (F ("Send SMS Succeed!\r\n"));
		                                }
                                    else
                                      {
                                        Serial.print (F("Send SMS failed!\r\n"));
			                                } // close the Else

                          } // Close the for 1
                } // Close the (EEPROM) Status Flag if (Rete presente)
      } // Close Theshold 200V Present
    

/* RESET GIORNALIERO 
Gestisce l'evento di avvenuto reset del GSM controllando giorno e l'ora
(il controllo viene effettuato ogni 15 Minuti) */
if (currentMillis - previousMillisora > intervalora) 
        {
            previousMillisora = currentMillis;
            gprs.getDateTime(locDateTime);
            Serial.print (F ("Verifica ogni 15 Min del RESET Data e Ora: "));
            Serial.println(locDateTime);
            Serial.println (F ("Ora chiama TimeToReset per verificare se è il momento di resettare"));
	
            if (TimeToReset() == true)
                {
	                Serial.print (F (" Devo fare Reset "));
                    initapp();
                } // close the if 2
      } // close the if 1

  
  } // close the loop function