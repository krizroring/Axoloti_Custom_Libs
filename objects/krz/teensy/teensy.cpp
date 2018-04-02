uint8_t *rxbuf; // Serial read buffer
uint8_t *txbuf; // Serial write buffer
uint8_t *values;

void setup (void){
    static uint8_t _txbuf[16] __attribute__ ((section (".sram2")));
    static uint8_t _rxbuf[4] __attribute__ ((section (".sram2")));
    txbuf = _txbuf;
    rxbuf = _rxbuf;

    static uint8_t _values[8] __attribute__ ((section (".sram2")));
    values = _values;
    // Send the current patch index
    sendPatchLoaded();
}

void loop (void) {
    // read all pending bytes
    while(!sdGetWouldBlock(&SD2)) {
        sdRead(&SD2,rxbuf, 4);

        if (rxbuf[1] == 0x2F) { // check for separator char
            switch(rxbuf[0]) {
            case 'P': {
                sendFileNames();
                break;
            }
            case 'L': {
                LoadPatchIndexed(rxbuf[3]);
                break;
            }
            case 'I': {
                sendPatchIndex();
                break;
            }
            case 'C': {
                sendControllers();
                break;
            }
            // add other cases here
            default: LogTextMessage("unhandled: 0x%X",rxbuf[0]);
            }
        }
    }

    checkControllerUpdates();

    chThdSleepMilliseconds(100);
}

// Sends the filenames from the index file to the serial connection
void sendFileNames() {
    FRESULT err;
    FIL FileObject;
    err = f_open( &FileObject, "/index.axb",  FA_READ | FA_OPEN_EXISTING  );

    if( err != FR_OK ) {
        f_close( &FileObject );
    } else {
        char byte;
        unsigned int bytes_read;

        char canReadChar = 1;
        char count = 0;

        while( !f_eof(&FileObject) ) {
            err = f_read( &FileObject, (uint8_t *) &byte, sizeof( char ), &bytes_read );

            if( err != FR_OK ) {
                break;
            }

            if (canReadChar == 1 && count == 0) {
                setTxBuffer(0x50);
            }

            if( byte == 0x2E ) { // '.'
                canReadChar = 0;
            }

            if ( canReadChar == 1 && count <= 14 ) {
                txbuf[count + 2] = byte;
                count++;
            }

            if( byte == 0x0A ) { // newline
                sendTxBuffer();
                count = 0; // reset count
                canReadChar = 1;
            }
        }

        f_close( &FileObject );

        endTransmission();
    }
}

// Send the patch loaded
void sendPatchLoaded() {
    setTxBuffer(0x4C);
    sendTxBuffer();

    endTransmission();
}

// Send the index of the current loaded patch
void sendPatchIndex() {
    setTxBuffer(0x49);
    txbuf[15] = GetIndexOfCurrentPatch() & 0xFF;
    sendTxBuffer();

    endTransmission();
}
// Send the controllers for the current loaded patch
void sendControllers() {
    for (int i = 0; i < ctrls; i++) {
        setTxBuffer(0x43);
        txbuf[2] = name[i][0];
        txbuf[3] = name[i][1];
        txbuf[4] = name[i][2];

        // default the controller refs
        values[i] = ctrl[i] >> 20;
        txbuf[15] = values[i];

        sendTxBuffer();
    }
    endTransmission();
}

// Check for a update of a controller and send it to the Teensy
void checkControllerUpdates() {
    bool update = false;

    for (int i = 0; i < ctrls; i++) {
        if(values[i] != ctrl[i] >> 20) {
            values[i] = ctrl[i] >> 20;

            setTxBuffer(0x63);
            txbuf[14] = i;
            txbuf[15] = values[i];
            sendTxBuffer();

            update = true;
        }
    }

    if(update == true) {
        endTransmission();
    }
}

// Ends the transmission
void endTransmission() {
    setTxBuffer(0x1B); // esc
    sendTxBuffer();
}

// Sets the control byte and filles the bufer with 'spaces'
void setTxBuffer(char _control) {
    std::fill_n( txbuf, 16, 0x20 ); // Fill the buffer with spaces

    txbuf[0] = _control; // Set the control byte
    txbuf[1] = 0x2F; // Set the separator byte to '/' for Patch
}

// Send the tx buffer over serial
void sendTxBuffer() {
    for (int i = 0; i< 16; i++) {
        LogTextMessage("Data 0x%X", txbuf[i]);
    }

    sdWrite(&SD2, txbuf, 16);
}
