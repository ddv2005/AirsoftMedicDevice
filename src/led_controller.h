#ifndef LED_CONTROLLER_H_
#define LED_CONTROLLER_H_
#include <Arduino.h>
#include <stdint.h>

#ifndef __packed
#define __packed  __attribute__((packed))
#endif

#ifndef byte_t
#define byte_t  uint8_t
#endif
typedef struct ledScript_t
{
  byte_t  size;
  byte_t  id;
  byte_t* data;
} __packed ledScript_t;

typedef struct ledScripts_t
{
  byte_t count;
  ledScript_t *scripts;
} __packed ledScripts_t;

typedef enum ledModes_t
{
  LM_OFF,
  LM_ON,
  LM_SCRIPT
} ledModes_t;

// Script Commands
#define LCSC_OFF    0x00
#define LCSC_ON     0x01
// Wait, 2 bytes ms
#define LCSC_WAIT   0x02
// Set Value, 1 byte - value
#define LCSC_SET    0x03
// 1 byte - value, 2 bytes - time in ms
#define LCSC_FADE   0x04
//  byte addr
#define LCSC_GOTO   0x05

#define LCS_TIME_MS(A) (byte_t)A & 0xFF, (byte_t)(((uint16_t)A)>>8)
#define LCS_GETTIME_MS(A, B) (((uint16_t)A & 0xFF) | (((uint16_t)B)<<8))
#define LCS_COLOR(R, G, B) (((uint32_t)B & 0xFF) | (((uint32_t)G & 0xFF)<<8) | (((uint32_t)R & 0xFF)<<16))

#define LCV_OFF 0

class ledController_c
{
protected:
  byte_t      m_pinR;
  byte_t      m_pinG;
  byte_t      m_pinB;
  byte_t      m_pwmChannel;

  ledModes_t  m_mode;
  ledScripts_t *m_scripts;
  ledScript_t  *m_currentScript;
  byte_t        m_currentScriptId;
  byte_t        m_currentScriptPos;
  uint32_t      m_scriptTimer;
  uint32_t      m_startValue;
  uint32_t      m_lastValue;
  uint32_t      m_color;
  
  void  setValue(uint32_t value)
  {
    m_lastValue = value;
    ledcWrite(m_pwmChannel,(value & 0xFF0000)>>16); 
    ledcWrite(m_pwmChannel+1,(value & 0xFF00)>>8); 
    ledcWrite(m_pwmChannel+2,(value & 0xFF)); 
  }

public:
  ledController_c(byte_t pinR,byte_t pinG, byte_t pinB, byte_t pwmChannel, ledScripts_t *scripts):m_scripts(scripts)
  {
    m_color = 0xFFFFFF;
    m_pinR = pinR;
    m_pinG = pinG;
    m_pinB = pinB;
    m_pwmChannel = pwmChannel;
    ledcAttachPin(pinR, pwmChannel);
    ledcAttachPin(pinG, pwmChannel+1);
    ledcAttachPin(pinB, pwmChannel+2);
    ledcSetup(pwmChannel,4000,8);
    ledcSetup(pwmChannel+1,4000,8);
    ledcSetup(pwmChannel+2,4000,8);
    m_currentScript = NULL;
    m_currentScriptId = 0;
    m_mode = LM_OFF;
    setValue(LCV_OFF);
  }
  
  void setColor(uint32_t color)
  {
    m_color = color;
  }

  void setON()
  {
//    if(m_mode != LM_ON)
    {
      m_mode = LM_ON;
      setValue(m_color);
    }
  };
  
  void setOFF()
  {
    if(m_mode != LM_OFF)
    {
      m_mode = LM_OFF;
      setValue(LCV_OFF);
    }
  };
  
  void runScript(byte_t  id)
  {
    //if(m_mode==LM_SCRIPT && m_currentScriptId==id) return;
    m_mode = LM_SCRIPT;
    m_currentScriptId = id;
    m_currentScript = NULL;
    m_currentScriptPos = 0;
    m_scriptTimer = 0;
    if(m_scripts)
    {
      for(byte_t i=0; i<m_scripts->count; i++)
      {
        if(m_scripts->scripts[i].id == id)
        {
          m_currentScript = &m_scripts->scripts[i];
        }
      }
    }
  }
 
  byte_t getCmdSize(byte_t cmd)
  {
    switch(cmd)
    {
      case LCSC_OFF:
        return 1;

      case LCSC_ON:
        return 4;

      case LCSC_SET:
      case LCSC_GOTO:
        return 2;

      case LCSC_WAIT:
        return 3;

      case LCSC_FADE:
        return 6;
    };
    return 0;
  } 

  byte_t approximateValue(byte_t value,byte_t start,uint32_t now,uint32_t time)
  {
  
    int16_t nv = start+((((int32_t)value-(int32_t)start)*(int32_t)(now-m_scriptTimer))/(int32_t)time);
    if(nv<0)
      nv = 0;
    else
      if(nv>255)
        nv = 255;
    return nv;
  }

  void loop()
  {
        if(m_mode==LM_SCRIPT)
        {
          if(m_currentScript)
          {
            while(true)
            {
              if(m_currentScriptPos<m_currentScript->size)
              {
                byte_t cmd = m_currentScript->data[m_currentScriptPos];
                byte_t cmd_size = getCmdSize(cmd);
                if((cmd_size>0)&&((m_currentScriptPos+cmd_size)<=m_currentScript->size))
                {
                  byte_t cont = 3;
                  uint16_t time;
                  uint32_t now;
                  switch(cmd)
                  {
                    case LCSC_OFF:
                      setValue(LCV_OFF);
                      break;
                      
                    case LCSC_ON:
                      setValue(LCS_COLOR(m_currentScript->data[m_currentScriptPos+1], m_currentScript->data[m_currentScriptPos+2], m_currentScript->data[m_currentScriptPos+3]));
                      break;

                    case LCSC_SET:
                      setValue(m_currentScript->data[m_currentScriptPos+1]);
                      break;
                    
                    case LCSC_GOTO:
                      m_currentScriptPos = m_currentScript->data[m_currentScriptPos+1];
                      cont = 0;
                      break;
              
                    case LCSC_WAIT:
                      {
                        time = LCS_GETTIME_MS(m_currentScript->data[m_currentScriptPos+1], m_currentScript->data[m_currentScriptPos+2]);
                        if(m_scriptTimer==0)
                        {
                          m_scriptTimer = millis();
                          cont = 2;
                        }
                        else
                        {
                          now = millis();
                          if((m_scriptTimer>now) || ((now-m_scriptTimer)>=time))
                          {
                            m_scriptTimer = 0;
                            cont = 1;
                          }
                          else
                            cont = 2;
                        }
                        break;
                      }
              
                    case LCSC_FADE:
                      {
                        time = LCS_GETTIME_MS(m_currentScript->data[m_currentScriptPos+4], m_currentScript->data[m_currentScriptPos+5]);
                        if(m_scriptTimer==0)
                        {
                          m_scriptTimer = millis();
                          m_startValue = m_lastValue;
                          cont = 2;
                        }
                        else
                        {
                          byte_t valueR = m_currentScript->data[m_currentScriptPos+1];
                          byte_t valueG = m_currentScript->data[m_currentScriptPos+2];
                          byte_t valueB = m_currentScript->data[m_currentScriptPos+3];
                          now = millis();
                          if((m_scriptTimer>now) || ((now-m_scriptTimer)>=time))
                          {
                            setValue(LCS_COLOR(valueR,valueG,valueB));
                            m_scriptTimer = 0;
                            cont = 1;
                          }
                          else
                          {
                            valueR = approximateValue(valueR,(m_startValue & 0xFF0000)>>16,now,time);
                            valueG = approximateValue(valueG,(m_startValue & 0xFF00)>>8,now,time);
                            valueB = approximateValue(valueB,(m_startValue & 0xFF),now,time);
                            setValue(LCS_COLOR(valueR,valueG,valueB));
                            cont = 2;
                          }
                        }
                        break;
                      }                    
                      break;
                  };
                  if(cont&1)
                  {
                    m_currentScriptPos += cmd_size;
                    if(m_currentScriptPos>=m_currentScript->size)
                      m_currentScriptPos = 0;
                  }
                  else
                  {
                    if(cont&2)
                    {
                      break;
                    }
                  }
                }
                else
                {
                  m_currentScriptPos = 0;
                  m_mode = LM_OFF;
                  setValue(LCV_OFF);
                  break;
                }
              }
              else
              {
                m_currentScriptPos = 0;
                break;
              }
            }
          }
        }
  }
};

#endif /* LED_CONTROLLER_H_ */
