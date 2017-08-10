/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * developed by swpark@nexell.co.kr
 */

package com.android.settings;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.RemoteException;
import android.os.SystemProperties;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceScreen;
import android.provider.Settings;
import android.provider.Settings.SettingNotFoundException;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.preference.CheckBoxPreference;

import java.util.ArrayList;

public class HDMISettings extends SettingsPreferenceFragment implements
Preference.OnPreferenceChangeListener {
    private static final String LOG_TAG = "HDMISettings";

    private static final String HDMI_RESOLUTION_PROPERTY = "persist.hwc.resolution";
    private static final String HDMI_MODE_PROPERTY = "hwc.hdmimode";

    private static final String HDMI_MODE_SECONDARY = "secondary";
    private static final String HDMI_MODE_PRIMARY = "primary";

    private static final String KEY_HDMI_RESOLUTION = "hdmi_resolution";

    private ListPreference mHDMIResolutionPreference;

    private int mCurResolution;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        addPreferencesFromResource(R.xml.hdmi_settings);

        mHDMIResolutionPreference = (ListPreference)findPreference(KEY_HDMI_RESOLUTION);

        mCurResolution = getResolution();
        mHDMIResolutionPreference.setValue(String.valueOf(mCurResolution));
        mHDMIResolutionPreference.setOnPreferenceChangeListener(this);

        if (HDMI_MODE_PRIMARY.equals(getHDMIMode()))
            mHDMIResolutionPreference.setEnabled(false);
        else
            mHDMIResolutionPreference.setEnabled(true);
    }

    public boolean onPreferenceChange(Preference preference, Object objValue) {
        final String key = preference.getKey();
        if (KEY_HDMI_RESOLUTION.equals(key)) {
            int value = Integer.parseInt((String)objValue);
            setResolution(value);
        }
        return true;
    }

    private void setResolution(int resolution) {
        if (resolution != mCurResolution) {
            mCurResolution = resolution;
            SystemProperties.set(HDMI_RESOLUTION_PROPERTY, Integer.toString(mCurResolution));
            Settings.System.putInt(mHDMIResolutionPreference.getContext().getContentResolver(), HDMI_RESOLUTION_PROPERTY, mCurResolution);
        }
    }

    private int getResolution() {
        String resolutionVal = SystemProperties.get(HDMI_RESOLUTION_PROPERTY, "18");
        Log.d(LOG_TAG, "hwc.resolution value: " + resolutionVal);
        return Integer.parseInt(resolutionVal);
    }

    private String getHDMIMode() {
        String hdmiMode = SystemProperties.get(HDMI_MODE_PROPERTY, "secondary");
        Log.d(LOG_TAG, "hwc.hdmimode: " + hdmiMode);
        return hdmiMode;
    }
}

