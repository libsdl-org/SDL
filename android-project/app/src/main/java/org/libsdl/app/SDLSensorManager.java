package org.libsdl.app;

import android.hardware.Sensor;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;

// This class coordinates synchronized access to sensor manager registration
//
// This prevents a java.util.ConcurrentModificationException exception on
// Android 16, specifically on the Samsung Tab S9 Ultra.

class SDLSensorManager
{
    static private SDLSensorManager mManager = new SDLSensorManager();

    public static void registerListener(SensorManager manager, SensorEventListener listener, Sensor sensor, int samplingPeriodUs) {
        mManager.RegisterListener(manager, listener, sensor, samplingPeriodUs);
    }

    public static void unregisterListener(SensorManager manager, SensorEventListener listener, Sensor sensor) {
        mManager.UnregisterListener(manager, listener, sensor);
    }

    private synchronized void RegisterListener(SensorManager manager, SensorEventListener listener, Sensor sensor, int samplingPeriodUs) {
        manager.registerListener(listener, sensor, samplingPeriodUs, null);
    }

    private synchronized void UnregisterListener(SensorManager manager, SensorEventListener listener, Sensor sensor) {
        manager.unregisterListener(listener, sensor);
    }
}

