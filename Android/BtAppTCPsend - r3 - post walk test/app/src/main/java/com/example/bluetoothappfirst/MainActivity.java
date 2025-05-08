package com.example.bluetoothappfirst;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.text.Editable;
import android.text.TextWatcher;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.ObjectWriter;

import java.io.IOException;
import java.util.Arrays;
import java.util.Random;
import java.util.Set;


public class MainActivity extends Activity {

    //used in debugger
    public static final String TAG = "myApp";

    public static MainActivity singleton = null;

    // Buttons
    public Button search;
    public Button gps;

    // Texts
    public TextView txtStatus; // log text area
    public TextView txtBtStatus; // log text area
    public TextView txtGpsStatus; // log text area

    public EditText txtName; // log text area
    public EditText txtCol; // log text area


    // BT
    private BluetoothAdapter mBTAdapter;
    private static final int BT_ENABLE_REQUEST = 10; // This is the code we use for BT Enable
    public BluetoothDevice dBBBT = null;
    public BluetoothConnection btCon = null;
    public Context btContext;

    // Extra variables to move to the next activity
    public static final String DEVICE_EXTRA = "com.example.bluetoothappfirst.SOCKET"; //
    public static final String DEVICE_UUID = "com.example.bluetoothappfirst.uuid"; // THESE VALUES ARE SENT ACROSS ACTIVITIES


    ///// TCP
    private TCPClient mTcpClient;

    // GPS
    public CurrentLocation thisGPS = new CurrentLocation();

    //Looping
    UIUpdater mUIUpdater = null;


    @SuppressLint("MissingPermission")
    @Override
    protected void onStart() {
        super.onStart();


        GenRandCol();

        //get a generic name from phone
        if (txtName.getText().toString().equals(""))
            txtName.setText(mBTAdapter.getName());

        thisGPS.onClick(); // auto start GPS


    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {


        // Keep awake - TEST REMOVING THIS AND CHECK CONTINOUS GPS DATA / BT
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);


        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);


        singleton = this;

        thisGPS.Setup();
        thisGPS.thisContext = this;
        btContext = this;

        //find layout items
        search = (Button) findViewById(R.id.btnScan); //the button in activity_main
        gps = (Button) findViewById(R.id.btnGPS); //the button in activity_main

        txtStatus = (TextView) findViewById(R.id.txtStatus); //the textview in activity_main
        txtStatus.setMovementMethod(new ScrollingMovementMethod()); // set it to scroll view

        txtBtStatus = (TextView) findViewById(R.id.txtBtStatus); //the textview in activity_main
        txtGpsStatus = (TextView) findViewById(R.id.txtGps); //the textview in activity_main

        txtName = (EditText) findViewById(R.id.etxtName); //the textview in activity_main
        txtCol = (EditText) findViewById(R.id.etxtCol); //the textview in activity_main


        mBTAdapter = BluetoothAdapter.getDefaultAdapter();


        // deal with Looping to server
        mUIUpdater = new UIUpdater(new Runnable() {
            @Override
            public void run() {
                // send a message to server
                thisGPS.sendData();
            }
        });


        // set on click listener for buttons

        // when search is clicked, we check if the phone has any bluetooth capability, if everything is ok, we then do action with bonded pairs
        search.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View arg0) {


                // ask for permission for location
                int permissionCheck = ContextCompat.checkSelfPermission(btContext, Manifest.permission.BLUETOOTH_SCAN);
                Log.d(TAG, "Permission Check scan: " + permissionCheck);
                if (permissionCheck != PackageManager.PERMISSION_GRANTED) {
                    // ask permissions here using below code
                    ActivityCompat.requestPermissions(MainActivity.singleton,
                            new String[]{Manifest.permission.BLUETOOTH_SCAN}, 4545);
                    Log.d(TAG, "Permission Check failed BT scan");
                    return;
                }

                // ask for permission for BT CONNECT AND SCAN
                permissionCheck = ContextCompat.checkSelfPermission(btContext, Manifest.permission.BLUETOOTH_CONNECT);
                Log.d(TAG, "Permission Check connect: " + permissionCheck);
                if (permissionCheck != PackageManager.PERMISSION_GRANTED) {
                    // ask permissions here using below code
                    ActivityCompat.requestPermissions(MainActivity.singleton,
                            new String[]{Manifest.permission.BLUETOOTH_CONNECT}, 4444);
                    Log.d(TAG, "Permission Check failed BT connect");
                    return;
                }


                if (mBTAdapter == null) {
                    //DEFAULT SHOW DEVICE NOT HAS BLUETOOTH
                    msg("Bluetooth not found");
                } else if (!mBTAdapter.isEnabled()) {
                    //TRY TO ENABLE BT
                    Intent enableBT = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
                    startActivityForResult(enableBT, BT_ENABLE_REQUEST);
                } else {
                    if (dBBBT == null) {
                        //SEARCH FOR DEVICES TO POPULATE LIST
                        msg("Listing BT Paired devices");
                        new SearchDevices().execute();
                    } else {
                        ConnectToBluetooth();
                    }
                }
            }
        });


        gps.setOnClickListener(new View.OnClickListener() {
            @SuppressLint("MissingPermission")
            @Override
            public void onClick(View arg0) {
                //msg("GPS clicked!");
                thisGPS.onClick();
            }
        });


        txtCol.addTextChangedListener(new TextWatcher() {

            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {
            }

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
            }

            @Override
            public void afterTextChanged(Editable editable) {
                //Toast.makeText(MainActivity.this, "AfterTextChanged!", Toast.LENGTH_LONG).show();
                MainActivity.singleton.TextChanged();
            }
        });


    }// OnCreate


    // Quick way to call the Toast
    public void msg(String str) {
        Toast.makeText(getApplicationContext(), str, Toast.LENGTH_SHORT).show();

        //System.out.println(str);
        Log.d("myApp", str);
    }

    public void msg(String str, boolean logOnly) {
        if (!logOnly)
            Toast.makeText(getApplicationContext(), str, Toast.LENGTH_SHORT).show();

        //System.out.println(str);
        Log.d("myApp", str);
    }

    public void TextUpdate(String s) {
        Log.d(TAG, s);
        txtStatus.setText(txtStatus.getText() + s + "\n");
    }


    public void TextChanged() {
        try {
            txtCol.setTextColor(Color.parseColor("#" + txtCol.getText().toString()));
        } catch (Exception e) {
            txtCol.setTextColor(Color.RED);
        }
    }

    public void GenRandCol() {

        char[] hexVals = ("0123456789ABCDEF").toCharArray();
        char[] hex = new char[6];

        for (int i = 0; i < 6; i++) {
            int r = new Random().nextInt(16);
            hex[i] = hexVals[r];
        }

        String out = new String(hex);

        txtCol.setText(out);

        TextChanged();

    }









        /*

        B L U E T O O T H

        */


    // Searches for paired devices. Doesn't do a scan! Only devices which are paired through Settings->Bluetooth

    // BB - My guess, doInBackground happens when .execute() is called. and after doInBackground finishes, onPostExecute is immediately called. (maybe in same thread?)
    private class SearchDevices extends AsyncTask<Void, String, BluetoothDevice> {

        @SuppressLint("MissingPermission")
        @Override
        protected BluetoothDevice doInBackground(Void... params) {
            // get list of bonded devices
            Set<BluetoothDevice> pairedDevices = mBTAdapter.getBondedDevices();

            for (BluetoothDevice device : pairedDevices) {
                //report what we found
                publishProgress("[Paired device]" + " " + device.getName());
                // return match
                if (device.getName().compareTo("BBBT") == 0) return device;
            }
            //set a default value for return
            return null;
        }


        @Override
        protected void onProgressUpdate(String... progress) {
            TextUpdate(progress[0]);
        }


        // https://developer.android.com/reference/android/os/AsyncTask
        @SuppressLint("MissingPermission")
        @Override
        protected void onPostExecute(BluetoothDevice device) {
            //super.onPostExecute(device);
            if (device != null) {
                MainActivity.singleton.dBBBT = device;
                txtBtStatus.setText(device.getName());
                TextUpdate("Main device: " + MainActivity.singleton.dBBBT);
                ConnectToBluetooth();
            } else {
                msg("No paired devices found, please pair your serial BT device and try again");
            }
        }


    }// Search devices


    public void ConnectToBluetooth() {
        if (btCon != null && btCon.isConnected) {
            Log.e(MainActivity.TAG, "Done PostExecute");
            btCon.Disconnect();
        } else {
            btCon = new BluetoothConnection(dBBBT);
        }
    }












    /*

        T C P

     */


    /// Connect to TCP server
    public class connectTask extends AsyncTask<String, String, TCPClient> {

        @Override
        protected TCPClient doInBackground(String... message) {

            //we create a TCPClient object and
            // 2 - creates the first class to hold TCP connection
            mTcpClient = new TCPClient(new TCPClient.OnMessageReceived() {
                @Override
                //here the messageReceived method is implemented
                //3 - Pass the method that will be called when a message is recieved - not sure if this keeps running indefinetely.
                public void messageReceived(String message) {
                    //this method calls the onProgressUpdate
                    publishProgress(message);
                }
            });
            // 4 - run the server listen indefinitely
            mTcpClient.run();

            return null;
        }

        @Override
        protected void onProgressUpdate(String... values) {
            // FROM SERVER
            // 3b[8b] - output of method called on message created
            TextUpdate(values[0]);

            if (values[0] != null)
                ProcessServerData(values[0]);
        }
    }

    public void ProcessServerData(String s) {

        // POLAR IN
        // D: [{"r":8,"a":143,"c":"DBE56F"}]
        ObjectMapper om = new ObjectMapper();
        PolarData[] data = null;

        //https://mkyong.com/java/jackson-convert-json-array-string-to-list/
        try {
            data = om.readValue(s, PolarData[].class);

            for (int i = 0; i < data.length; i++) {
                TextUpdate(data[i].a + " " + data[i].r + " " + data[i].c);
            }
        } catch (IOException e) {
            msg("Error parsing JSON-san");
            e.printStackTrace();
            return;
        }

        // CONVERT JSON TO SIMPLE STRING FOR ARDUINO PROCESSING
        // https://forum.arduino.cc/t/serial-input-basics-updated/382007
        //          £r8a143cDBE56F,r15a90,r25a180,r25a270,*

        String arduinoOut = "£-";

        if (data != null) {
            for (int i = 0; i < data.length; i++) {
                arduinoOut += "U-a-" + data[i].a + "-r-" + data[i].r + "-c-" + data[i].c + "-,";
            }
        }
        arduinoOut += "-*\n";

        Log.e(MainActivity.TAG, "Sending: " + arduinoOut);


        // TO BT
        if (btCon != null) {
            btCon.SendMsg(arduinoOut);
        }


    }







    /*

        G P S

     */

    public class CurrentLocation {

        public int REQUEST_GPS_CODE = 4242;

        public Context thisContext;

        private LocationManager locationMangaer = null;
        private LocationListener locationListener = null;


        public double latitude = 0;
        public double longitude = 0;
        public float accuracy = -1;
        public String jsonOut = "";

        private Boolean flag = false;


        public void Setup() {

            locationMangaer = (LocationManager) getSystemService(Context.LOCATION_SERVICE);

        }


        //@SuppressLint("MissingPermission")
        public void onClick() {
            Log.d(TAG, "onClick");


            TextUpdate("Move your device to see the changes in coordinates.");


            locationListener = new MyLocationListener();


            Log.d(TAG, "OS No: " + android.os.Build.VERSION.SDK_INT);

            // ask for permission for overlay
            if (android.os.Build.VERSION.SDK_INT >= 23 && !Settings.canDrawOverlays(MainActivity.singleton)) {   //Android M Or Over
                Intent intent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION, Uri.parse("package:" + getPackageName()));
                startActivityForResult(intent, 4040);
                return;
            }


            // ask for permission for location
            int permissionCheck = ContextCompat.checkSelfPermission(thisContext, Manifest.permission.ACCESS_FINE_LOCATION);
            Log.d(TAG, "Permission Check: " + permissionCheck);
            if (permissionCheck != PackageManager.PERMISSION_GRANTED) {
                // ask permissions here using below code
                ActivityCompat.requestPermissions(MainActivity.singleton,
                        new String[]{Manifest.permission.ACCESS_FINE_LOCATION},
                        REQUEST_GPS_CODE);
                Log.d(TAG, "Permission Check failed LOCATION");
                return;
            }


            Log.d(TAG, "Start location Callback");
            locationMangaer.requestLocationUpdates(LocationManager.GPS_PROVIDER, 5000, 2, locationListener);
            txtGpsStatus.setText("Updating");


            // Start looper class
            mUIUpdater.startUpdates();
        }


        /*----------Listener class to get coordinates ------------- */
        private class MyLocationListener implements LocationListener {

            @Override
            public void onLocationChanged(Location loc) {

                /*Toast.makeText(thisContext, "Location changed : " +
                                "Lat: " + loc.getLatitude() +
                                " Lng: " + loc.getLongitude() +
                                " Acc: " + loc.getAccuracy(),
                        Toast.LENGTH_SHORT).show();*/

                longitude = loc.getLongitude();
                latitude = loc.getLatitude();
                accuracy = loc.getAccuracy();

                String s = longitude + " " + latitude + " " + accuracy;
                MainActivity.singleton.TextUpdate(s);
            }

            @Override
            public void onProviderDisabled(String provider) {
                // TODO Auto-generated method stub
            }

            @Override
            public void onProviderEnabled(String provider) {
                // TODO Auto-generated method stub
            }

            @Override
            public void onStatusChanged(String provider, int status, Bundle extras) {
                // TODO Auto-generated method stub
            }

        }// LocationListener Class


        public void sendData() {
            //only send if we have data
            if (latitude == 0 || longitude == 0) return;


            //check if accurate enough to send
            if (accuracy > 30) {
                Log.d(TAG, "TOO LOW ACCURACY - " + accuracy);
                return;
            }


            GpsDataOut d = new GpsDataOut();
            d.name = txtName.getText().toString();
            d.col = txtCol.getText().toString();
            d.lat = latitude;
            d.lon = longitude;


            ObjectWriter ow = new ObjectMapper().writer().withDefaultPrettyPrinter();
            Log.d(TAG, "Try JSON");

            try {
                jsonOut = ow.writeValueAsString(d);
                Log.d(TAG, "Json: " + jsonOut);
            } catch (Exception e) {
                Log.d(TAG, "Json Error: " + e);
                return;
            }

            // send a message to server
            new MainActivity.connectTask().execute("");
        }


        public class GpsDataOut {
            public double lat, lon;
            public String name, col;
        }


    }// CurrentLocation Class





        /*

        I N V O K E   R E P E A T I N G   S E T U P

         */

    public class UIUpdater {
        // Create a Handler that uses the Main Looper to run in
        private Handler mHandler = new Handler(Looper.getMainLooper());

        private Runnable mStatusChecker;
        private int UPDATE_INTERVAL = 5000;

        /**
         * Creates an UIUpdater object, that can be used to
         * perform UIUpdates on a specified time interval.
         *
         * @param uiUpdater A runnable containing the update routine.
         */
        public UIUpdater(final Runnable uiUpdater) {
            mStatusChecker = new Runnable() {
                @Override
                public void run() {
                    // Run the passed runnable
                    uiUpdater.run();
                    // Re-run it after the update interval
                    mHandler.postDelayed(this, UPDATE_INTERVAL);
                }
            };
        }

        /**
         * The same as the default constructor, but specifying the
         * intended update interval.
         *
         * @param uiUpdater A runnable containing the update routine.
         * @param interval  The interval over which the routine
         *                  should run (milliseconds).
         */
        public UIUpdater(Runnable uiUpdater, int interval) {
            this(uiUpdater);
            UPDATE_INTERVAL = interval;
        }

        /**
         * Starts the periodical update routine (mStatusChecker
         * adds the callback to the handler).
         */
        public synchronized void startUpdates() {
            mStatusChecker.run();
        }

        /**
         * Stops the periodical update routine from running,
         * by removing the callback.
         */
        public synchronized void stopUpdates() {
            mHandler.removeCallbacks(mStatusChecker);
        }
    }


}