/**
 * Modified https://playground.arduino.cc/Code/HomeEasy?action=sourceblock&num=3
 * by
 * Barnaby Gray 12/2008
 * Peter Mead   09/2009
 */

#include <Arduino.h>

// function prototype allowing the function to be used before it is defined.
static char *dec2binWzerofill(unsigned long dec, unsigned int bitLength);
void printBinary(unsigned long inNumber, unsigned int digits = 32);

int rxPin = 2;

void setup()
{
    pinMode(rxPin, INPUT);
    Serial.begin(9600);
}

void loop()
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

void printBinary(unsigned long inNumber, unsigned int digits = 32)
{
    /*
  for (int b = digits-1; b >= 0; b--)
  {
    Serial.print(bitRead(inNumber, b));
  }
  */
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