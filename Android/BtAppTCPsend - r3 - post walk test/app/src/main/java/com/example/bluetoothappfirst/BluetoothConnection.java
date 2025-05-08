package com.example.bluetoothappfirst;

import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.os.AsyncTask;
import android.util.Log;

import java.io.IOException;
import java.io.InputStream;
import java.util.UUID;

public class BluetoothConnection {


    private BluetoothDevice mDevice; // made final
    private final UUID mDeviceUUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB"); // made final
    private BluetoothSocket mBTSocket;
    private ReadInput mReadThread = null;



    public boolean isConnected = false;










    //Init setup of class
    public BluetoothConnection(BluetoothDevice d) {

        Log.e(MainActivity.TAG,"New BT class");
        // cache local device
        mDevice = d;
        Log.e(MainActivity.TAG,"passed in " + d.toString());
        MainActivity.singleton.msg("Done BT setup", true);


        //connect to device
        new ConnectBT().execute();

    }


    public void SendMsg(String s) {

        //Log.e(MainActivity.TAG,"Sending: " + s);

        try {
            mBTSocket.getOutputStream().write(s.getBytes());
            Log.e(MainActivity.TAG, "Sent BT msg!");
        } catch (IOException e) {
            e.printStackTrace();
            MainActivity.singleton.msg("Error Sending BT", true);
            isConnected = false;
            new ConnectBT().execute();
        }

    }


    // thread to deal with incoming connection
    private class ReadInput implements Runnable {

        private boolean bStop = false;
        private Thread t;

        public ReadInput() {
            t = new Thread(this, "run");
            t.start();
        }


        public boolean isRunning() {
            return t.isAlive();
        }


        @Override
        public void run() {

            InputStream inputStream;

            try {
                inputStream = mBTSocket.getInputStream(); // create a new stream object
                while (!bStop) { // run until false

                    byte[] buffer = new byte[256]; // make a receiving buffer area
                    if (inputStream.available() > 0) { // if we have some data on the incoming
                        inputStream.read(buffer); // put incoming bytes into the buffer


                        //This is needed to count how many characters we have in buffer  |  http://stackoverflow.com/a/8843462/1287554
                        int i = 0;
                        for (i = 0; i < buffer.length && buffer[i] != 0; i++) {
                        }

                        final String strInput = new String(buffer, 0, i); //construct the chars into a string

                        MainActivity.singleton.msg(strInput, true); // do something with this string
                    }

                    Thread.sleep(500); // reduce resource usage
                }
            } catch (Exception e) { // different exceptions in each catch (removed multiple catches)
                e.printStackTrace();
            }
        }

        public void stop() {
            bStop = true;
        }
    }










    private class ConnectBT extends AsyncTask<Void, Void, Boolean> {


        // * onPreExecute - This method is run on the UI before the task starts and is responsible for any setup that needs to be done.
        // * doInBackground - This is the code for the actual task you want done off the main thread. It will be run on a background thread and not disrupt the UI.
        // * onProgressUpdate - This is a method that is run on the UI thread and is meant for showing the progress of a task, such as animating a loading bar.
        // * onPostExecute - This is a method that is run on the UI after the task is finished.


        @SuppressLint("MissingPermission")
        @Override
        protected Boolean doInBackground(Void... devices) {
            Log.e(MainActivity.TAG,"Connect BG");
            try {
                if (mBTSocket == null || !isConnected) {
                    Log.e(MainActivity.TAG,"If");
                    mBTSocket = mDevice.createInsecureRfcommSocketToServiceRecord(mDeviceUUID);
                    Log.e(MainActivity.TAG,"New Socket");
                    BluetoothAdapter.getDefaultAdapter().cancelDiscovery();
                    Log.e(MainActivity.TAG,"Cancel discovery");
                    mBTSocket.connect();
                    Log.e(MainActivity.TAG,"After connect");
                }
            } catch (IOException e) {
                Log.e(MainActivity.TAG,"Caught: " + e);
                return false;
            }
            return true;
        }


        @Override
        protected void onPostExecute(Boolean result) {
            Log.e(MainActivity.TAG,"Post execude connected");

            if (result) {
                MainActivity.singleton.msg("Connected to device");
                isConnected = true;
                mReadThread = new ReadInput(); // Kick off input reader

            } else {
                MainActivity.singleton.msg("Could not connect to device.");
            }

            Log.e(MainActivity.TAG,"Done PostExecute");
        }

    }// Connect




    public void Disconnect(){
        new DisConnectBT().execute();
    }


    private class DisConnectBT extends AsyncTask<Void, Void, Void> {


        @Override
        protected Void doInBackground(Void... params) {

            if (mReadThread != null) {
                mReadThread.stop();

                // Wait until it stops
                while (mReadThread.isRunning()) {
                }

                mReadThread = null;
            }

            try {
                mBTSocket.close();
            } catch (IOException e) {
            }

            return null;
        }

        @Override
        protected void onPostExecute(Void result) {
            super.onPostExecute(result);
            isConnected = false;
        }


    } // Disconnect


}
