int messageIndex;

messageIndex = gprs.isSMSunread();

if (messageIndex > 0) {

    int concatID;
    int totalParts;
    int partNumber;

    int result = gprs.getSMSPartInfo_PDU(
        messageIndex,
        &concatID,
        &totalParts,
        &partNumber
    );

    if (result == 1) {

        Serial.print("SMS concatenato ");
        Serial.print(partNumber);
        Serial.print("/");
        Serial.print(totalParts);

        Serial.print(" ID=");
        Serial.println(concatID);

        if (partNumber == 1) {
            Serial.println("PRIMA PARTE");
        }
    }
    else if (result == 0) {

        Serial.println("SMS normale");
    }
    else {

        Serial.println("Errore lettura PDU");
    }

    /*
     * Continua a leggere il testo come facevi prima.
     */
    if (gprs.readSMS(messageIndex,
                     message,
                     MESSAGE_LENGTH,
                     phone,
                     datetime)) {

        Serial.println(message);
    }
}
