using System;
using System.Text;
using System.Net;
using System.Net.Sockets;
using Newtonsoft.Json;
using System.Collections.Generic;

class ASync
{

    static TcpListener Listener; // Reference to current listening port
    
    public static List<GpsDataStore> users = new List<GpsDataStore>();// Server data



    // Entry point for class
    static public void AsyncStart()
    {
        try
        {
            //Console.WriteLine("Any Ip: " + IPAddress.Any);
            Console.WriteLine($"Starting server on: {serv.myIP}:8001"); 
            Console.WriteLine();

            // Start listening to incoming requests
            Listener = new TcpListener(IPAddress.Any, 8001);
            Listener.Start();
            StartTCPClientListener();
        }
        catch (Exception) { }
    }







    // Setup callback for Async Task
    static private void StartTCPClientListener()
    {
        Listener.BeginAcceptTcpClient(new AsyncCallback(HandleTCPClientConnection), null);
    }
    


    // Actual callback function on connected client
    static private void HandleTCPClientConnection(IAsyncResult ar)
    {
        try
        {
            // Accept client
            TcpClient client = Listener.EndAcceptTcpClient(ar); 
            Console.WriteLine($"Got a connection from {client.Client.RemoteEndPoint}");

            StartCommunication(client); // Handle client request

            // End client
            client.Close();
            client.Dispose();
        }
        catch (Exception) { }

        
        StartTCPClientListener(); // Setup next listening event
    }





    // Main function of client handling
    static void StartCommunication(TcpClient client)
    {
        Console.WriteLine("Waiting for message");


        try
        {
            int bytesRead; // Message length
            byte[] buffer = new byte[512]; // buffer

            
            bytesRead = client.GetStream().Read(buffer, 0, buffer.Length); // Read and record how much data received           
            serv.msgRecieved = Encoding.Default.GetString(buffer, 0, bytesRead); // Convert bytes to string
            Console.WriteLine("Recieved: " + serv.msgRecieved);


            GpsDataIn dIn = null; // Reference to parse data into
            try
            {
                dIn = JsonConvert.DeserializeObject<GpsDataIn>(serv.msgRecieved); // Convert the data into class object
            }
            catch (Exception e)
            {
                Console.WriteLine("Json Error: " + e.ToString());
            }


            // Update or add this client's data based on name provided then return a polar co-ordinate for all other clients     
            string returnString = "";
            if (dIn != null)
            {
                returnString = ProcessData(client, dIn); // Return relative user's polar co-ords
            }

            // EndLine character for Android
            returnString += "\n";


            // Send return to client
            Console.WriteLine("Sending: " + returnString);
            Send(returnString, client.GetStream());


        }
        catch (Exception)
        {
            Console.WriteLine("Timed Out");
        }

    }

    // Check if the data given is by an existing user, add new user reference if not.
    // Return a string of each polar co-ordinates for each other user
    static public string ProcessData(TcpClient client, GpsDataIn dIn)
    {
        
        GpsDataStore curUser = null; // Reference for active client's data

        // Check if client's name exists in users
        for (int i = 0; i < users.Count; i++)
        {
            //Console.WriteLine($"Comapring {users[i].name} with {dIn.name}");
            if (users[i].name == dIn.name)
            {
                //matched name
                users[i].lat = dIn.lat;
                users[i].lon = dIn.lon;
                users[i].col = dIn.col;
                users[i].last = DateTime.Now.Ticks;

                //we are done with updating
                curUser = users[i];
                break;
            }
        }

        // Add user if not existing
        if (curUser == null)
        {
            //Console.WriteLine($"New user");
            curUser = new GpsDataStore(dIn);
            users.Add(curUser);
        }




        // Get relative polar co-ordinates from each user's lat/lon
        List<PolarData> returnData = new List<PolarData>();

        for (int i = 0; i < users.Count; i++)
        {
            if (users[i] != curUser)
            {
                returnData.Add(users[i].PrintPolar(curUser));
            }
        }


        string json = ""; // Reference for return message string
        try
        {
            json = JsonConvert.SerializeObject(returnData, Formatting.None);
            //Console.WriteLine(json);
        }
        catch (Exception)
        {
            json = "NO_DATA";
        }


        return json;
    }


    



    // Send message to client
    static protected void Send(string message, NetworkStream clientStream)
    {
        byte[] temp = Encoding.Default.GetBytes(message);
        clientStream.Write(temp, 0, temp.Length);
        clientStream.Flush();
    }

    // Terminate listening server.
    public static void Stop()
    {
        // Problem: Exception because "HandleTCPClientConnection" returns wrong IAsyncResult
        if (Listener != null)
            Listener.Stop();
    }
}


