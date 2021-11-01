package com.ddv.airsoftmedicdeviceapp;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

public class amdViewModel extends ViewModel
{
    private MutableLiveData<amdDeviceState> m_deviceState = new MutableLiveData<amdDeviceState>();
    public void setAmdDevice(amdDeviceState item)
    {
        m_deviceState.setValue(item);
    }

    public LiveData<amdDeviceState> getAmdDevice()
    {
        return  m_deviceState;
    }
}