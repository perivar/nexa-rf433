/**
 * Parts modified https://playground.arduino.cc/Code/HomeEasy?action=sourceblock&num=3
 * by
 * Barnaby Gray 12/2008
 * Peter Mead   09/2009
 * Other parts taken from RC-Switch make it Nexa compatible
 */

#include <Arduino.h>

#define DEBUG //If you comment this line, debug statements are turned off

// function prototype allowing the function to be used before it is defined.
static char *dec2binWzerofill(unsigned long dec, unsigned int bitLength);
void printBinary(unsigned long inNumber, unsigned int digits = 32);

// RC-Switch variables
#define RCSWITCH_MAX_CHANGES 250
const unsigned int nSeparationLimit = 4300;
unsigned int timings[RCSWITCH_MAX_CHANGES];
char receivedBits[RCSWITCH_MAX_CHANGES];
volatile unsigned int changeCount = 0;
unsigned long nReceivedValue = NULL;
unsigned int nReceivedBitlength = 0;
unsigned int nReceivedDelay = 0;
unsigned int nReceivedProtocol = 0;
int nReceiveTolerance = 60;

// input pin for the 433 Mhz module
int rxPin = 2;

void setup()
{
    Serial.begin(9600);
    pinMode(rxPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(rxPin), interrupt_handler, CHANGE); // 0 is D2
}

void loop()
{
    //loop_locked();

    if (available())
    {
        Serial.print("Received ");
        Serial.print(nReceivedValue);
        Serial.print(" / ");
        Serial.print(nReceivedBitlength);
        Serial.print("bit ");
        Serial.print("Protocol: ");
        Serial.println(nReceivedProtocol);

        resetAvailable();
    }
}

void loop_locked()
{
    int i = 0;
    unsigned long t = 0;

    byte prevBit = 0;
    byte bit = 0;

    unsigned long sender = 0;
    unsigned long senderbin1 = 0;
    unsigned long senderbin2 = 0;
    bool group = false;
    bool on = false;
    unsigned int command = 0;

    // latch 1
    while ((t < 9480 || t > 10350))
    {
        t = pulseIn(rxPin, LOW, 1000000);
    }

    // latch 2
    while (t < 2550 || t > 2700)
    {
        t = pulseIn(rxPin, LOW, 1000000);
    }

    // data
    while (i < 64)
    {
        t = pulseIn(rxPin, LOW, 1000000);

        if (t > 200 && t < 365)
        {
            bit = 0;
        }
        else if (t > 1000 && t < 1360)
        {
            bit = 1;
        }
        else
        {
            i = 0;
            break;
        }

        if (i % 2 == 1)
        {
            if ((prevBit ^ bit) == 0)
            { // must be either 01 or 10, cannot be 00 or 11
                i = 0;
                break;
            }

            if (i < 53)
            { // first 26 data bits
                sender <<= 1;
                sender |= !prevBit;

                if (i < 31)
                {
                    senderbin1 <<= 1;
                    senderbin1 |= !prevBit;
                    senderbin1 <<= 1;
                    senderbin1 |= !bit;
                }
                else
                {
                    senderbin2 <<= 1;
                    senderbin2 |= !prevBit;
                    senderbin2 <<= 1;
                    senderbin2 |= !bit;
                }
            }
            else if (i == 53)
            { // 26th data bit
                group = prevBit;
            }
            else if (i == 55)
            { // 27th data bit
                on = prevBit;
            }
            else
            { // last 4 data bits
                command <<= 1;
                command |= prevBit;
            }
        }

        prevBit = bit;
        ++i;
    }

    // interpret message
    if (i > 0)
    {
        printResult(sender, senderbin1, senderbin2, group, on, command);
    }
}

static inline unsigned int diff(int A, int B)
{
    return abs(A - B);
}

bool receiveProtocolNexa(unsigned int changeCount)
{
    // there are no Nexa protocols with less than x bits
    // ignore very short transmissions: no device sends them, so this must be noise
    if (changeCount < 8)
        return false;

    unsigned long code = 0;
    const unsigned long delay = timings[0] / 41;
    const unsigned long delayTolerance = delay * nReceiveTolerance / 100;

    // store bits in the receivedBits char array
    unsigned int nReceivedBitsPos = 0;
    unsigned int nTmpReceivedBitlength = (changeCount - 3) / 2; // 3 starting bits needs to be removed

#ifdef DEBUG
    Serial.println();
    Serial.print("Detecting protocol using ");
    Serial.print(changeCount);
    Serial.print(" timings. Delay: ");
    Serial.print(delay);
    Serial.print(". Delay tolerance: ");
    Serial.print(delayTolerance);
    Serial.print(". Bit length: ");
    Serial.print(nTmpReceivedBitlength);
    Serial.println();

    Serial.print("Raw data: ");
    for (int i = 0; i <= changeCount; i++)
    {
        Serial.print(timings[i]);
        Serial.print(",");
    }
    Serial.println();
#endif

    for (int i = 1; i < changeCount; i = i + 2)
    {
        // check if have sync bit (T + 10T)
        if (diff(timings[i], delay) < delayTolerance &&
            diff(timings[i + 1], delay * 11) < delayTolerance)
        {
#ifdef DEBUG
            Serial.print("sync ");
#endif
            // found sync, but don't do anything
        }
        // or 1 bit (T + T)
        else if (diff(timings[i], delay) < delayTolerance &&
                 diff(timings[i + 1], delay) < delayTolerance)
        {
#ifdef DEBUG
            Serial.print("1");
#endif
            // store one in receivedBits char array
            receivedBits[nReceivedBitsPos++] = '1';
        }
        // or 0 bit (T + 5T)
        else if (diff(timings[i], delay) < delayTolerance &&
                 diff(timings[i + 1], delay * 5) < delayTolerance)
        {
#ifdef DEBUG
            Serial.print("0");
#endif
            // store zero in receivedBits char array
            receivedBits[nReceivedBitsPos++] = '0';
        }
        else
        {
            // Failed
            return false;
        }
    }

#ifdef DEBUG
    if (nReceivedBitsPos > 0)
    {
        Serial.println();
        Serial.print("verf ");
        for (int j = 0; j < nReceivedBitsPos; j++)
        {
            Serial.print(receivedBits[j]);
        }
        Serial.println();
    }
#endif

    // Decode the data stream into logical bytes.
    // The data part on the physical link is coded so that every logical bit is sent as
    // two physical bits, where the second one is the inverse of the first one.
    // '0' => '01'
    // '1' => '10'
    // Example the logical datastream 0111 is sent over the air as 01101010.
    // I.e. keep every second byte
    if (nReceivedBitsPos > 0)
    {
        for (int k = 0; k < nReceivedBitsPos; k = k + 2)
        {
            code <<= 1;
            if (receivedBits[k] == '1')
            {
                code |= 1;
            }
        }
    }

#ifdef DEBUG
    if (code > 0)
    {
        Serial.print("lgic ");
        Serial.println(dec2binWzerofill(code, nTmpReceivedBitlength / 2));
    }
#endif

    // ignore low bit counts as there are no devices sending only x bit values => noise
    if (changeCount > 8)
    {
        nReceivedValue = code;
        nReceivedBitlength = nTmpReceivedBitlength;
        nReceivedDelay = delay;
        nReceivedProtocol = 10;
        return true;
    }

    return false;
}

bool available()
{
    return nReceivedValue != NULL;
}

void resetAvailable()
{
    nReceivedValue = NULL;
}

void interrupt_handler()
{
    static unsigned int duration = 0;
    static unsigned int changeCount = 0;
    static unsigned long lastTime = 0;
    static unsigned int repeatCount = 0;

    long curTime = micros();
    duration = curTime - lastTime;

    if (duration > nSeparationLimit)
    {
        // A long stretch without signal level change occurred. This could
        // be the gap between two transmission.
        if (diff(duration, timings[0]) < 200)
        {
            // This long signal is close in length to the long signal which
            // started the previously recorded timings; this suggests that
            // it may indeed by a a gap between two transmissions (we assume
            // here that a sender will send the signal multiple times,
            // with roughly the same gap between them).
            repeatCount++;
            changeCount--;
            if (repeatCount == 2)
            {
                if (receiveProtocolNexa(changeCount) == false)
                {
                    // failed
                }
                repeatCount = 0;
            }
        }
        changeCount = 0;
    }

    // detect overflow
    if (changeCount >= RCSWITCH_MAX_CHANGES)
    {
        changeCount = 0;
        repeatCount = 0;
    }

    timings[changeCount++] = duration;
    lastTime = curTime;
}

// print results from the locked read loop
void printResult(unsigned long sender, unsigned long senderbin1, unsigned long senderbin2, bool group, bool on, unsigned int command)
{
    Serial.print("sender ");
    Serial.println(sender);
    printBinary(sender);
    Serial.println();

    printBinary(senderbin1);
    printBinary(senderbin2);
    Serial.println();

    if (group)
    {
        Serial.println("group command");
    }
    else
    {
        Serial.println("no group");
    }

    if (on)
    {
        Serial.println("on");
    }
    else
    {
        Serial.println("off");
    }

    Serial.print("command ");
    Serial.println(command);

    Serial.println();
}

static void printBinary(unsigned long inNumber, unsigned int digits = 32)
{
    Serial.print(dec2binWzerofill(inNumber, digits));
}

static char *dec2binWzerofill(unsigned long dec, unsigned int bitLength)
{
    static char bin[64];
    unsigned int i = 0;

    while (dec > 0)
    {
        bin[32 + i++] = ((dec & 1) > 0) ? '1' : '0';
        dec = dec >> 1;
    }

    for (unsigned int j = 0; j < bitLength; j++)
    {
        if (j >= bitLength - i)
        {
            bin[j] = bin[31 + i - (j - (bitLength - i))];
        }
        else
        {
            bin[j] = '0';
        }
    }
    bin[bitLength] = '\0';

    return bin;
}