package com.ddv.airsoftmedicdeviceapp;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.IntentFilter;
import android.nfc.Tag;
import android.nfc.tech.Ndef;
import android.os.Bundle;

import com.google.android.material.bottomnavigation.BottomNavigationView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.lifecycle.ViewModelProvider;
import androidx.navigation.NavController;
import androidx.navigation.Navigation;
import androidx.navigation.ui.AppBarConfiguration;
import androidx.navigation.ui.NavigationUI;
import android.nfc.NfcAdapter;
import android.nfc.tech.NfcV;
import android.util.Log;
import android.widget.Toast;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class MainActivity extends AppCompatActivity
{
    private static final String TAG = "AirsoftMedicDevice";

    private amdViewModel viewModel;
    private amdDeviceState m_deviceState = new amdDeviceState();
    private NfcAdapter m_nfc;
    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        viewModel =
                new ViewModelProvider(this).get(amdViewModel.class);
        if(viewModel.getAmdDevice().getValue()==null)
            viewModel.setAmdDevice(m_deviceState);
        else
            m_deviceState = viewModel.getAmdDevice().getValue();

        BottomNavigationView navView = findViewById(R.id.nav_view);
        AppBarConfiguration appBarConfiguration = new AppBarConfiguration.Builder(
                R.id.navigation_home)
                .build();
        NavController navController = Navigation.findNavController(this, R.id.nav_host_fragment);
        NavigationUI.setupActionBarWithNavController(this, navController, appBarConfiguration);
        NavigationUI.setupWithNavController(navView, navController);
        m_nfc = NfcAdapter.getDefaultAdapter(this);
        if(m_nfc!=null)
            handleIntent(getIntent());
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "Called onResume");

        if (m_nfc != null) {
            setupForegroundDispatch(this,m_nfc);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(TAG, "Called onPause");

        if (m_nfc != null) {
            m_nfc.disableForegroundDispatch(this);
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        handleIntent(intent);
    }

    public static void setupForegroundDispatch(final Activity activity, NfcAdapter adapter) {
        final Intent intent = new Intent(activity.getApplicationContext(), activity.getClass());
        intent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);

        final PendingIntent pendingIntent = PendingIntent.getActivity(activity.getApplicationContext(), 0, intent, PendingIntent.FLAG_CANCEL_CURRENT);

        IntentFilter[] filters = new IntentFilter[1];
        String[][] techList = new String[][]{ new String[] {NfcV.class.getSimpleName()}};

        // Notice that this is the same filter as in our manifest.
        filters[0] = new IntentFilter();
        filters[0].addAction(NfcAdapter.ACTION_TAG_DISCOVERED);
        adapter.enableForegroundDispatch(activity, pendingIntent, filters, techList);
    }

    private String ByteArrayToHexString(byte [] inarray) {
        int i, j, in;
        String [] hex = {"0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"};
        String out= "";

        for(j = 0 ; j < inarray.length ; ++j)
        {
            in = (int) inarray[j] & 0xff;
            i = (in >> 4) & 0x0f;
            out += hex[i];
            i = in & 0x0f;
            out += hex[i];
        }
        return out;
    }

    private void handleIntent(Intent intent) {
        String action = intent.getAction();
        if(m_nfc.ACTION_TAG_DISCOVERED.equals(action) || m_nfc.ACTION_TECH_DISCOVERED.equals(action))
        {
            Tag tag = intent.getParcelableExtra(NfcAdapter.EXTRA_TAG);
            processDevice(tag,ByteArrayToHexString(intent.getByteArrayExtra(NfcAdapter.EXTRA_ID)));
        }
    }

    private void writeToTag(NfcV tag,int address, int size, byte[] data) throws IOException
    {
        Log.d(TAG,"Write TAG");
        byte tagId[] = tag.getTag().getId();
        byte[] cmd = new byte[]{
                (byte)0x22,
                (byte)0x24,
                (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00,  // placeholder for UID
                0, 0
        };
        System.arraycopy(tagId, 0, cmd, 2, 8);
        byte[] cmdfull = new byte[cmd.length + 16];
        System.arraycopy(cmd, 0, cmdfull, 0, cmd.length);
        int blocks = size/4;
        for(int i=0; i<blocks; i+=4)
        {
            int n = blocks-i;
            if(n>4) n = 4;
            cmdfull[10]=(byte)((address+i) & 0x00FF);
            cmdfull[11]=(byte)((n - 1) & 0x0ff);
            System.arraycopy(data, i*4, cmdfull, cmd.length, n*4);
            Log.d(TAG, "Writing " + ByteArrayToHexString(cmdfull));
            byte[] response = tag.transceive(Arrays.copyOfRange(cmdfull,0,cmd.length+n*4));
            Log.d(TAG, "Write result " + ByteArrayToHexString(response));
        }
    }
    private byte[] readFromTag(NfcV tag,int address, int blocks) throws IOException
    {
        byte tagId[] = tag.getTag().getId();
        byte[] cmd = new byte[]{
                (byte)0x22,
                (byte)0x23,
                (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x00,  // placeholder for UID
                (byte)(address & 0x00FF), (byte)((blocks - 1) & 0x0ff)
        };
        System.arraycopy(tagId, 0, cmd, 2, 8);
        byte[] response = tag.transceive(cmd);
        return Arrays.copyOfRange(response,1,response.length);
    }

    private byte[]  readTagSettings(NfcV vtag) throws IOException
    {
        return readFromTag(vtag,128/4,64/4);
    }

    private void processDevice(Tag tag, String uuid)
    {
        Log.d(TAG, "processDevice UUID: "+uuid);
        String[] techList = tag.getTechList();
        String searchedTech = NfcV.class.getName();

        for (String tech : techList) {
            if (searchedTech.equals(tech)) {
                NfcV vtag = NfcV.get(tag);
                if(vtag!=null)
                {
                    try
                    {
                        vtag.connect();
                        if(m_deviceState.getWriting()>0)
                        {
                            switch (m_deviceState.getWriting())
                            {
                                case 1:
                                {
                                    if (uuid.equals(m_deviceState.getUuid()))
                                    {
                                        writeToTag(vtag, 128 / 4, Math.min(m_deviceState.getMemory().length, 32), m_deviceState.getMemory());
                                        byte[] data = readTagSettings(vtag);
                                        m_deviceState.setMemory(data);
                                        viewModel.setAmdDevice(m_deviceState);
                                        int duration = Toast.LENGTH_SHORT;
                                        Toast toast = Toast.makeText(getApplicationContext(), "Saved", duration);
                                        toast.show();
                                        m_deviceState.clearProgressDialog();
                                        m_deviceState.setWriting(0);
                                        m_deviceState.setChanged(false);
                                    } else
                                    {
                                        int duration = Toast.LENGTH_SHORT;
                                        Toast toast = Toast.makeText(getApplicationContext(), "Wrong Device", duration);
                                        toast.show();
                                    }
                                    break;
                                }

                                case 2:
                                {
                                    if (uuid.equals(m_deviceState.getUuid()))
                                    {
                                        byte[] memory = m_deviceState.getMemory();
                                        if(memory.length>32)
                                        {
                                            byte [] data = Arrays.copyOfRange(memory,32,memory.length);
                                            writeToTag(vtag, (128+32) / 4, data.length, data);
                                            data = readTagSettings(vtag);
                                            m_deviceState.setMemory(data);
                                            viewModel.setAmdDevice(m_deviceState);
                                            int duration = Toast.LENGTH_SHORT;
                                            Toast toast = Toast.makeText(getApplicationContext(), "Revivals counter reseted", duration);
                                            toast.show();
                                            m_deviceState.clearProgressDialog();
                                            m_deviceState.setWriting(0);
                                        }
                                    } else
                                    {
                                        int duration = Toast.LENGTH_SHORT;
                                        Toast toast = Toast.makeText(getApplicationContext(), "Wrong Device", duration);
                                        toast.show();
                                    }
                                    break;
                                }

                            }
                        }
                        else
                        {
                            boolean canLoad = !uuid.equals(m_deviceState.getUuid()) || !m_deviceState.getChanged();
                            if (canLoad)
                            {
                                m_deviceState.setUuid(uuid);
                                byte[] data = readTagSettings(vtag);
                                m_deviceState.setMemory(data);
                                viewModel.setAmdDevice(m_deviceState);
                            } else
                            {
                                int duration = Toast.LENGTH_SHORT;
                                Toast toast = Toast.makeText(getApplicationContext(), "Reset/Save changes first!", duration);
                                toast.show();
                            }
                        }
                        vtag.close();
                    } catch (IOException e)
                    {
                        e.printStackTrace();
                    }
                }
                break;
            }
        }
    }

}