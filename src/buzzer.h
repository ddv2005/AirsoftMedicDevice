#pragma once
#include <Arduino.h>
#include <driver/rtc_io.h>
#include "concurrency\Thread.h"
#include "utils.h"

#define BE_PLAY 1
#define BE_STOP 2

#define LED_CHANNEL 1
//#define PWM_BUZZER

struct sBuzzerEvent
{
    int16_t action;
    int16_t param1;
    int16_t param2;
    int16_t param3;
    int16_t param4;
};

typedef cProtectedQueue<sBuzzerEvent> cBuzzerEvents;

class cBuzzer
{
protected:
    typedef concurrency::ThreadFunctionTemplate<cBuzzer> cBuzzerThreadFunction;
    int8_t m_pin;    
    cBuzzerThreadFunction *m_thread;
    cBuzzerEvents m_events;
    int8_t  m_toneState;
    int64_t m_toneStart;
    int16_t m_toneFreq;
    int16_t m_toneCount;
    int16_t m_toneDuration;
    int16_t m_toneIdle;

    void startTone(int16_t freq)
    {
#ifdef PWM_BUZZER
        ledcWriteTone(LED_CHANNEL, freq);
#else
        digitalWrite(m_pin,0);
#endif        
    }

    void stopToneInternal()
    {
#ifdef PWM_BUZZER
        ledcWrite(LED_CHANNEL, 0);
#else
        digitalWrite(m_pin,1);
#endif                
    }

    void stopTone()
    {
        stopToneInternal();
        m_toneFreq = 0;
        m_toneStart = 0;
        m_toneCount = 0;
        m_toneDuration = 0;
        m_toneIdle = 0;
        m_toneState = 0;
    }
    void buzzerThread(cBuzzerThreadFunction *thread)
    {
        while (true)
        {
            m_events.lock();
            if(m_events.size())
            {
                while(m_events.size()>1)
                    m_events.pop();
                sBuzzerEvent cmd = m_events.front();
                m_events.pop();
                m_events.unlock();
                switch (cmd.action)
                {
                    case BE_PLAY:
                    {
                        stopTone();
                        m_toneState = 1;
                        m_toneStart = 0;
                        m_toneFreq = cmd.param1;
                        m_toneCount = cmd.param2;
                        m_toneDuration = cmd.param3;
                        m_toneIdle = cmd.param4;
                        break;
                    }

                    case BE_STOP:
                    {
                        stopTone();
                        break;
                    }
                default:
                    break;
                }
            }
            else
                m_events.unlock();
            switch (m_toneState)
            {
                case 1: //Start tone
                    if(m_toneCount>0)
                    {
                        m_toneState = 2;
                        m_toneCount--;
                        m_toneStart = millis();
                        startTone(m_toneFreq);
                    }
                    else
                    {
                        stopTone();
                    }
                    break;

                case 2: //Tone in progress
                {
                    int64_t now = millis();
                    if((now-m_toneStart)>m_toneDuration)
                    {
                        stopToneInternal();
                        m_toneState = 3;
                        m_toneStart = now;
                    }
                    break;
                }

                case 3: //Idle in progress
                {
                    int64_t now = millis();
                    if((now-m_toneStart)>(m_toneIdle-25))
                    {
                        if(m_toneCount>0)
                        {
                            m_toneState = 1;
                        }
                        else
                        {
                            stopTone();
                        }
                    }
                    break;
                }
            default:
                break;
            }
            delay(25);
        }
    }
public:
    cBuzzer(int8_t pin):m_pin(pin),m_events(10)
    {
        m_thread = NULL;
        pinMode(m_pin, OUTPUT);
#ifdef PWM_BUZZER        
        ledcSetup(LED_CHANNEL, 1000, 8);
        ledcAttachPin(m_pin, LED_CHANNEL);
#endif        
        stopTone();
    }

    void begin()
    {
        m_thread = new cBuzzerThreadFunction(*this, &cBuzzer::buzzerThread);
        m_thread->start("Buzzer", 4096, tskIDLE_PRIORITY+1, 0);
    }

    void forceStopTone()
    {
        stopTone();
    }

    void sendEvent(const sBuzzerEvent &event)
    {
        m_events.push_item(event);
    }
};