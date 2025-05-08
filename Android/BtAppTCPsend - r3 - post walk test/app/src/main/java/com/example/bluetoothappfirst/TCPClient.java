package com.example.bluetoothappfirst;

import android.telephony.TelephonyManager;
import android.util.Log;

import com.fasterxml.jackson.databind.ObjectMapper;

import java.io.*;
import java.net.InetAddress;
import java.net.Socket;
import java.util.concurrent.TimeoutException;

public class TCPClient {

    //public boolean sending = false;

    private String serverMessage;
    public static final String SERVERIP = "92.4.155.247"; //your computer IP address
    public static final int SERVERPORT = 8001;
    private OnMessageReceived mMessageListener = null;




    PrintWriter out;
    BufferedReader in;


    /**
     * Constructor of the class. OnMessagedReceived listens for the messages received from server
     */
    public TCPClient(OnMessageReceived listener) {
        mMessageListener = listener;
    }

    //Declare the interface. The method messageReceived(String message) will must be implemented in the MyActivity
    //class at on asyncTask doInBackground
    // Interface is here so that a reference to a method can be used as a variable
    public interface OnMessageReceived {
        public void messageReceived(String message);
    }


    // 4a - run from the Async task first called on mainActivity create
    public void run() {


        try {
            //here you must put your computer's IP address.
            InetAddress serverAddr = InetAddress.getByName(SERVERIP);

            Log.e("myApp", "C: Connecting...");

            //create a socket to make the connection with the server
            Socket socket = new Socket(serverAddr, SERVERPORT);

            socket.setSoTimeout(4000);

            try {

                //send the message to the server
                // 5 - make a connection to server, not sending any messages
                out = new PrintWriter(new BufferedWriter(new OutputStreamWriter(socket.getOutputStream())), true);
                Log.e("myApp", "C: Connection request complete.");


                //sends the message to the server
                // 9 - send a message to the server
                Log.e("myApp", "Send Message!");
                sendMessage();




                //receive the message which the server sends back
                // 6 - attempt to read anything from the server
                in = new BufferedReader(new InputStreamReader(socket.getInputStream()));

                //in this while the client listens for the messages sent by the server
                // 7 - indefinite Async Running listening on server messages
                try {
                    Log.e("myApp", "C: Inside run tag.");
                    serverMessage = in.readLine();

                    Log.e("myApp", "C: After readLine.");
                    if (serverMessage != null && mMessageListener != null) {
                        Log.e("myApp", "C: If statements.");
                        //call the method messageReceived from MyActivity class
                        // 8 - call the method dealing with the message
                        Log.e("myApp", "S: Received Message: '" + serverMessage + "'");
                        mMessageListener.messageReceived(serverMessage);
                        Log.e("myApp", "C: Done rec.");
                    }
                    serverMessage = null;
                } catch (Exception e) {
                    Log.e("myApp", "C: Caught Message recieve.");
                }


            } catch (Exception e) {
                Log.e("myApp", "S: Error", e);
            } finally {
                //the socket must be closed. It is not possible to reconnect to this socket
                // after it is closed, which means a new socket instance has to be created.
                Log.e("myApp", "C: Closing Socket");
                socket.close();
            }
        } catch (Exception e) {
            Log.e("myApp", "C: Error", e);
            Log.e("myApp", "C: Timeout?", e);
        }

    }// Run







    /**
     * Sends the message entered by client to the server
     */
    // 9a - send a message to the server socket
    public void sendMessage() {
        //Log.e("myApp", "C: Attempt to send.");

        try {
            if (out != null && !out.checkError()) {
                //Log.e("myApp", "C: Ok to send.");
                out.write(String.valueOf(MainActivity.singleton.thisGPS.jsonOut));
                out.flush();
                Log.e("myApp", "C: Sent!.");
            }
        } catch (Exception e) {
            Log.e("myApp", "C: Error Send: " + e);
        }

        MainActivity.singleton.thisGPS.longitude = 0;
        MainActivity.singleton.thisGPS.latitude = 0;

    }



}