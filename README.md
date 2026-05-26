**SMS Manager for Power monitoring**

&nbsp; **Specifiche:**

&nbsp; - OK Verifica corretta registrazione

&nbsp; - OK Reinizializzazione ogni 24 ore (ogni giorno) ad ora prestabilita

&nbsp; - OK invio SMS mancanza energia fino a 3 numeri

&nbsp; - OK invio SMS riattivazione energia fino a 3 numeri

&nbsp; - OK Comando SMS (S) per stato Power Supply e SMS a numero richiedente

&nbsp; + Messaggio SMS per richiedente non autorizzato

&nbsp; - OK Salvataggio stato in memoria non volatile (EEPROM O FLASH)

\- OK da Master. Prevedere la richiesta SMS (CMD N) per vedere quanti e quali numeri sono impostati + Messaggio SMS per richiedente non autorizzato

&nbsp; - NON Ancora (disabilitazione o abilitazione notifica a numero da richiesta SMS)

\- OK Cancellazione numeri ausiliari (disabilitazione o abilitazione notifica a TUTTI i numeri da richiesta SMS)

&nbsp; - OK Cancellazione numeri ausiliari

&nbsp; - NON Ancora set/reset pin uscita da SMS numero richiedente abilitato

&nbsp; - NON Ancora Stato pin ingresso su richiesta SMS a numero richiedente abilitato

&nbsp; - OK Verifica Indice telefoni ausiliari per evitare sovrapposizioni in input (INDEX OVERLAP)

&nbsp; - OK Prevedere SMS di conferma comandi (richiesta eseguita per il n.)

&nbsp; ..........................

**Scopo del codice**

Il programma:

Monitora la tensione di rete tramite un trasformatore e la libreria EmonLib.

\- Notifica via SMS a numeri autorizzati (Master + fino a 3 ausiliari) i seguenti eventi:

\- Mancanza di rete (tensione < 180V)

\- Ripresa rete (tensione > 200V)

Gestisce comandi via SMS:

\- **M+numero** → Modifica numero Master

\- **A\[n\]+numero** → Aggiunge/Sostituisce numero ausiliario

\- **D** → Cancella numeri ausiliari

\- **N** → Elenca numeri autorizzati

\- **S** → Invia stato tensione

\- Salva e legge i numeri autorizzati su EEPROM per persistenza.

**Comandi SMS**

&nbsp; **Numeri GSM**

&nbsp; Italia totale cifre 12 senza +

&nbsp; Germania totale cifre 13 senza +

&nbsp; Svezia totale cifre 11 senza +

&nbsp; Finlandia totale cifre 10 senza +

&nbsp; **"M+393391255597"** Sostituisci cellulare autorizzato Master con un altro

&nbsp; **"A1+393391255597"** AGGIUNGI/SOSTITUISCI cellulare in posizione....

&nbsp; **"D"** CANCELLA tutti i cellulari ausiliari (non il Master)

&nbsp; **"N"** Elenca tutti i numeri autorizzati - solo per il Master

&nbsp; EEPROM.read(5) Indicatore Rete presente (0) Rete assente (1)

&nbsp; **1st Power On**

&nbsp; - inizialmente la EEPROM non ha numeri (tutti "")

&nbsp; - Definire il numero Master (M)

&nbsp; - Una volta definito il numero master è possibile definire il primo numero ausiliario (A1)

&nbsp; e poi il secondo numero Ausiliario (A2)

&nbsp; - Con "D" il MASTER può CANCELLARE tutti i cellulari eccetto il numero Master autorizzato (0)