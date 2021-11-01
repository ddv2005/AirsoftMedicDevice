package com.ddv.airsoftmedicdeviceapp;
import android.app.ProgressDialog;
import android.os.Parcel;
import android.os.Parcelable;

import java.util.Arrays;

public class amdDeviceState implements Parcelable
{
    private String m_uuid="";
    private int m_medicTime;
    private int m_maxRevivalsCount;
    private int m_flags;
    private int m_startupPin;
    private int m_setupPin;
    private int m_currentRevivals;
    private byte[] m_memory = new byte[0];
    private byte[] m_memory_original = new byte[0];
    private boolean m_changed = false;
    private int m_writing = 0;
    private ProgressDialog m_progressDialog = null;

    public amdDeviceState()
    {
    }

    private amdDeviceState(Parcel in)
    {
        m_uuid = in.readString();
        in.readByteArray(m_memory);
        in.readByteArray(m_memory_original);
        m_changed = in.readInt()!=0;
        readFromMemory();
    }

    public Boolean isValid()
    {
        return m_uuid!=null && !m_uuid.isEmpty();
    }

    protected int memoryGetInt(int address)
    {
        if(m_memory!=null)
        {
            if(m_memory.length>=(address+4))
                return java.nio.ByteBuffer.wrap(m_memory,address,4).order(java.nio.ByteOrder.LITTLE_ENDIAN).getInt();
        }
        return 0;
    }

    protected void memoryPutInt(int address, int value)
    {
        if(m_memory!=null)
        {
            if(m_memory.length>=(address+4))
            {
                m_memory[address] = (byte)(value & 0xFF);;
                m_memory[address+1] = (byte)((value >> 8) & 0xFF);
                m_memory[address+2] = (byte)((value >> 16) & 0xFF);
                m_memory[address+3] = (byte)((value >> 24) & 0xFF);
            }
        }
    }

    public boolean checkAndSetChanged()
    {
        boolean result = false;
        if(m_memory!=null && m_memory_original!=null)
        {
            result = !Arrays.equals(Arrays.copyOfRange(m_memory,0,32),Arrays.copyOfRange(m_memory_original,0,32));
            setChanged(result);
        }
        return result;
    }

    public void readFromMemory()
    {
        m_medicTime = memoryGetInt(4);
        m_maxRevivalsCount = memoryGetInt(8);
        m_flags = memoryGetInt(12);
        m_startupPin  = memoryGetInt(16);
        m_setupPin = memoryGetInt(20);
        m_currentRevivals = memoryGetInt(60);
    }

    public void writeToMemory()
    {
        memoryPutInt(4, m_medicTime);
        memoryPutInt(8, m_maxRevivalsCount);
        memoryPutInt(12, m_flags);
        memoryPutInt(16, m_startupPin);
        memoryPutInt(20, m_setupPin);
        memoryPutInt(60, m_currentRevivals);
    }

    public void setProgressDialog(ProgressDialog dlg)
    {
        m_progressDialog = dlg;
    }

    public void clearProgressDialog()
    {
        if(m_progressDialog!=null)
            m_progressDialog.cancel();
        m_progressDialog = null;
    }

    public void reset()
    {
        m_memory = m_memory_original.clone();
        readFromMemory();
        m_changed = false;
    }

    public void setMemory(byte[] data)
    {
        m_changed = false;
        m_memory = data;
        m_memory_original = m_memory.clone();
        readFromMemory();
    }

    public byte[] getMemory()
    {
        return m_memory;
    }

    public static final Creator<amdDeviceState> CREATOR = new Creator<amdDeviceState>() {
        @Override
        public amdDeviceState createFromParcel(Parcel in) {
            return new amdDeviceState(in);
        }

        @Override
        public amdDeviceState[] newArray(int size) {
            return new amdDeviceState[size];
        }
    };
    @Override
    public int describeContents()
    {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel parcel, int i)
    {
        writeToMemory();
        parcel.writeString(m_uuid);
        parcel.writeByteArray(m_memory);
        parcel.writeByteArray(m_memory_original);
        parcel.writeInt(m_changed?1:0);
    }

    public boolean getChanged()
    {
        return m_changed;
    }

    public void setChanged(boolean v)
    {
        m_changed = v;
    }

    public int getWriting()
    {
        return m_writing;
    }

    public void setWriting(int v)
    {
        m_writing = v;
    }

    public int getMedicTime()
    {
        return m_medicTime;
    }

    public void setMedicTime(int m_medicTime)
    {
        this.m_medicTime = m_medicTime;
    }

    public int getCurrentRevivals()
    {
        return m_currentRevivals;
    }

    public void setCurrentRevivals(int v)
    {
        this.m_currentRevivals = v;
    }

    public int getMaxRevivalsCount()
    {
        return m_maxRevivalsCount;
    }

    public void setMaxRevivalsCount(int m_maxRevivalsCount)
    {
        this.m_maxRevivalsCount = m_maxRevivalsCount;
    }

    public int getFlags()
    {
        return m_flags;
    }

    public void setFlags(int m_flags)
    {
        this.m_flags = m_flags;
    }

    public int getStartupPin()
    {
        return m_startupPin;
    }

    public void setStartupPin(int m_startupPin)
    {
        this.m_startupPin = m_startupPin;
    }

    public int getSetupPin()
    {
        return m_setupPin;
    }

    public void setSetupPin(int m_setupPin)
    {
        this.m_setupPin = m_setupPin;
    }

    public String getUuid()
    {
        return m_uuid;
    }

    public void setUuid(String m_uuid)
    {
        this.m_uuid = m_uuid;
    }
}
