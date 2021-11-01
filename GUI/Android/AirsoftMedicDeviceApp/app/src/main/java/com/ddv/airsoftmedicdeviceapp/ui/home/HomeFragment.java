package com.ddv.airsoftmedicdeviceapp.ui.home;

import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputFilter;
import android.text.Spanned;
import android.text.TextWatcher;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.Switch;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProvider;

import com.ddv.airsoftmedicdeviceapp.R;
import com.ddv.airsoftmedicdeviceapp.amdDeviceState;
import com.ddv.airsoftmedicdeviceapp.amdViewModel;

public class HomeFragment extends Fragment
{

    private amdViewModel viewModel;
    private TextView m_uuidLabel;
    private EditText m_revivalEdit;
    private EditText m_medicTimeEdit;
    private EditText m_startupPinEdit;
    private EditText m_setupPinEdit;
    private TextView m_currentRevivalsLabel;
    private Switch m_askStartupPinEdit;
    private Switch m_askSetupPinEdit;
    private Button m_saveButton;
    private Button m_resetButton;
    private Button m_resetRevivalsButton;
    private boolean m_changing = false;

    public class InputFilterMinMax implements InputFilter
    {

        private int min, max;

        public InputFilterMinMax(int min, int max) {
            this.min = min;
            this.max = max;
        }

        public InputFilterMinMax(String min, String max) {
            this.min = Integer.parseInt(min);
            this.max = Integer.parseInt(max);
        }

        @Override
        public CharSequence filter(CharSequence source, int start, int end, Spanned dest, int dstart, int dend) {
            try {
                int input = Integer.parseInt(dest.toString() + source.toString());
                if (isInRange(min, max, input))
                    return null;
            } catch (NumberFormatException nfe) { }
            return "";
        }

        private boolean isInRange(int a, int b, int c) {
            return b > a ? c >= a && c <= b : c >= b && c <= a;
        }
    }
    public View onCreateView(@NonNull LayoutInflater inflater,
                             ViewGroup container, Bundle savedInstanceState)
    {
        viewModel =
                new ViewModelProvider(requireActivity()).get(amdViewModel.class);
        viewModel.getAmdDevice().observe(getViewLifecycleOwner(), item -> {
            updateView();
            updateState();
        });
        View root = inflater.inflate(R.layout.fragment_home, container, false);
        m_uuidLabel = root.findViewById(R.id.idUUIDLabel);
        m_revivalEdit = root.findViewById(R.id.idRevivalsEdit);
        m_revivalEdit.setFilters(new InputFilter[]{ new InputFilterMinMax("0", "1000")});

        m_medicTimeEdit = root.findViewById(R.id.idMedicTimeEdit);
        m_medicTimeEdit.setFilters(new InputFilter[]{ new InputFilterMinMax("1", "1000")});

        m_startupPinEdit = root.findViewById(R.id.idStartupPinEdit);
        m_setupPinEdit = root.findViewById(R.id.idSetupPinEdit);

        m_askStartupPinEdit = root.findViewById(R.id.idAskStartupPinEdit);
        m_askSetupPinEdit = root.findViewById(R.id.idAskSetupPinEdit);

        m_saveButton = root.findViewById(R.id.idSaveButton);
        m_resetButton = root.findViewById(R.id.idResetButton);

        m_currentRevivalsLabel = root.findViewById(R.id.idCurrentRevivalsLabel);
        m_resetRevivalsButton = root.findViewById(R.id.idResetRevivalsButton);

        updateView();
        m_revivalEdit.addTextChangedListener(m_changedWatcher);
        m_medicTimeEdit.addTextChangedListener(m_changedWatcher);
        m_startupPinEdit.addTextChangedListener(m_changedWatcher);
        m_setupPinEdit.addTextChangedListener(m_changedWatcher);
        m_askSetupPinEdit.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener()
        {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean b)
            {
                if(!m_changing)
                {
                    saveEditors();
                    updateState();
                }
            }
        });
        m_askStartupPinEdit.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener()
        {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean b)
            {
                if (!m_changing)
                {
                    saveEditors();
                    updateState();
                }
            }
        });

        updateState();;

        m_resetButton.setOnClickListener(new View.OnClickListener()
        {
            @Override
            public void onClick(View view)
            {
                viewModel.getAmdDevice().getValue().reset();
                updateView();
                updateState();
            }
        });

        m_saveButton.setOnClickListener(new View.OnClickListener()
        {
            @Override
            public void onClick(View view)
            {
                processSave(1);
            }
        });

        m_resetRevivalsButton.setOnClickListener(new View.OnClickListener()
        {
            @Override
            public void onClick(View view)
            {
                amdDeviceState device = viewModel.getAmdDevice().getValue();
                device.setCurrentRevivals(0);
                device.writeToMemory();
                processSave(2);
            }
        });
        return root;
    }

    private void processSave(int writeType)
    {
        ProgressDialog dialog = new ProgressDialog(getContext());
        dialog.setMessage("Tap Device");
        dialog.setIndeterminate(true);
        dialog.setCanceledOnTouchOutside(true);
        dialog.setButton(DialogInterface.BUTTON_NEGATIVE, "Cancel", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.dismiss();
            }
        });
        viewModel.getAmdDevice().getValue().setProgressDialog(dialog);
        viewModel.getAmdDevice().getValue().setWriting(writeType);
        dialog.show();
    }

    private boolean getDeviceChanged()
    {
        return viewModel.getAmdDevice().getValue().getChanged();
    }

    private TextWatcher m_changedWatcher = new TextWatcher()
    {
        @Override
        public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2)
        {

        }

        @Override
        public void onTextChanged(CharSequence charSequence, int i, int i1, int i2)
        {
            if(!m_changing)
            {
                saveEditors();
                updateState();
            }
        }

        @Override
        public void afterTextChanged(Editable editable)
        {

        }
    };

    public void setEditorsEnabled(Boolean b)
    {
        m_medicTimeEdit.setEnabled(b);
        m_revivalEdit.setEnabled(b);
        m_askStartupPinEdit.setEnabled(b);
        m_startupPinEdit.setEnabled(b);
        m_askSetupPinEdit.setEnabled(b);
        m_setupPinEdit.setEnabled(b);
        m_resetRevivalsButton.setEnabled(b);
    }

    public void updateState()
    {
        amdDeviceState device =  viewModel.getAmdDevice().getValue();
        setEditorsEnabled(device.isValid());
        m_saveButton.setEnabled(getDeviceChanged());
        m_resetButton.setEnabled(getDeviceChanged());
    }

    public void saveEditors()
    {
        amdDeviceState device = viewModel.getAmdDevice().getValue();
        try
        {
            if(m_medicTimeEdit.getText().toString().isEmpty())
                return;
            device.setMedicTime(Integer.valueOf(m_medicTimeEdit.getText().toString()) * 1000);
            device.setMaxRevivalsCount(Integer.valueOf(m_revivalEdit.getText().toString()));
            device.setStartupPin(Integer.valueOf(m_startupPinEdit.getText().toString()));
            device.setSetupPin(Integer.valueOf(m_setupPinEdit.getText().toString()));
            int flags = 0;
            if (m_askStartupPinEdit.isChecked())
                flags |= 1;
            if (m_askSetupPinEdit.isChecked())
                flags |= 2;
            device.setFlags(flags);
            device.writeToMemory();
            device.checkAndSetChanged();
        } catch (Exception e)
        {
            e.printStackTrace();
        }
    }


    public void updateView()
    {
        m_changing = true;
        amdDeviceState device =  viewModel.getAmdDevice().getValue();
        m_uuidLabel.setText(device.getUuid());
        m_medicTimeEdit.setText(Integer.toString(device.getMedicTime()/1000));
        m_revivalEdit.setText(Integer.toString(device.getMaxRevivalsCount()));
        m_askStartupPinEdit.setChecked((device.getFlags() & 1) == 1);
        m_askSetupPinEdit.setChecked((device.getFlags() & 2) == 2);
        m_setupPinEdit.setText(Integer.toString(device.getSetupPin()));
        m_startupPinEdit.setText(Integer.toString(device.getStartupPin()));
        m_currentRevivalsLabel.setText(Integer.toString(device.getCurrentRevivals()));
        m_changing = false;
    }
}