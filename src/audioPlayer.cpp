#include "audioPlayer.h"


cAudioPlayer::cAudioPlayer(AudioOutput *out):m_out(out),m_events(10)
{
    m_thread = NULL;
    m_idle = false;
}

void cAudioPlayer::stopPlayer()
{
    m_currentList.clear();
    if(m_mp3)
    {
        m_mp3->stop();
        delete m_mp3;
        m_mp3 = NULL;
    }
    if(m_currentFile)
    {
        delete m_currentFile;
        m_currentFile = NULL;
    }
}

void cAudioPlayer::begin()
{
    m_thread = new cAudioPlayerThreadFunction(*this, &cAudioPlayer::apThread);
    m_thread->start("AP", 4096, tskIDLE_PRIORITY+10, 0);
    m_idle = true;
}

void cAudioPlayer::apThread(cAudioPlayerThreadFunction *thread)
{
    std::list<std::string> currentList;
    m_currentFile = NULL;
    m_mp3 = NULL;
    
        while (true)
        {
            m_events.lock();
            if(m_events.size())
            {
                while(m_events.size()>1)
                    m_events.pop();
                sAudioPlayerEvent cmd = m_events.front();
                m_events.pop();
                m_events.unlock();
                switch (cmd.eid)
                {
                    case APE_PLAY_LIST:
                    {
                        stopPlayer();
                        m_currentList = cmd.list;
                        m_idle = false;
                        break;
                    }

                    case APE_STOP:
                    {
                        stopPlayer();
                        m_idle = true;
                        break;
                    }
                default:
                    break;
                }
            }
            else
                m_events.unlock();

            if(m_mp3)
            {
                if(!m_mp3->loop())
                {
                    m_mp3->stop();
                    delete m_mp3;
                    m_mp3 = NULL;
                    if(m_currentFile)
                    {
                        delete m_currentFile;
                        m_currentFile = NULL;
                    }
                }
            }
            if((m_mp3==NULL)&&(m_currentList.size()>0))
            {
                auto currentFile = m_currentList.front();
                m_currentList.pop_front();
                m_currentFile = new AudioFileSourceSPIFFS(currentFile.c_str());
                m_mp3 = new AudioGeneratorMP3();
                m_mp3->begin(m_currentFile, m_out);
            }
            else
                if((m_mp3==NULL) && !m_idle)
                    m_idle = true;

            delay(2);
        }
}

bool cAudioPlayer::isIdle()
{
    return m_idle;
}

void cAudioPlayer::sayNumber(uint16_t n, const char *file)
{
  sAudioPlayerEvent event;
  event.eid = APE_PLAY_LIST;
  if(n<=20)
    event.list.push_back(("/"+String(n)+".mp3").c_str());
  else 
  {
    String str = String(n);
    if(n%10==0)
        event.list.push_back(("/"+String(n-n/100*100) +".mp3").c_str());
    else
    {
        event.list.push_back(("/"+String(n/10-n/100*10) +"x.mp3").c_str());
        event.list.push_back(("/"+String(n-n/10*10) +".mp3").c_str());
    }
  }
  if(file)
    event.list.push_back(file);
  m_idle = false;
  sendEvent(event);
}

void cAudioPlayer::playFile(const char *file)
{
  sAudioPlayerEvent event;
  event.eid = APE_PLAY_LIST;
  event.list.push_back(file);
  m_idle = false;
  sendEvent(event);
}

void cAudioPlayer::playFile2(const char *file1,const char *file2)
{
  sAudioPlayerEvent event;
  event.eid = APE_PLAY_LIST;
  event.list.push_back(file1);
  event.list.push_back(file2);
  m_idle = false;
  sendEvent(event);
}

void cAudioPlayer::playStop()
{
  sAudioPlayerEvent event;
  event.eid = APE_STOP;
  sendEvent(event);
}
