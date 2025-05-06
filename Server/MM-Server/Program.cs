/*   Server Program    */

using System;
using System.Net;
using System.Net.Sockets;
using System.Net.NetworkInformation;

public class serv
{

    public static IPAddress myIP;    // Active IP address      
    public static string msgRecieved;  // Data received


    //Find Host IP Address
    public static string GetLocalIPv4(NetworkInterfaceType _type)
    {
        foreach (NetworkInterface item in NetworkInterface.GetAllNetworkInterfaces())
        {
            if (item.NetworkInterfaceType == _type && item.OperationalStatus == OperationalStatus.Up)
            {
                foreach (UnicastIPAddressInformation ip in item.GetIPProperties().UnicastAddresses)
                {
                    if (ip.Address.AddressFamily == AddressFamily.InterNetwork)
                    {
                        return (ip.Address.ToString());
                    }
                }
            }
        }
        return "";
    }



    // Entry point
    public static void Main()
    {
        string cIP = GetLocalIPv4(NetworkInterfaceType.Wireless80211); // Use local m/c IP address 

        if (cIP == "")
        {
            Console.WriteLine("Invalid IP address");
        }
        else
        {
            myIP = IPAddress.Parse(cIP);
            ASync.AsyncStart();  // Start listening Async
        }


        while (true) ;
        Console.WriteLine("any key to close");
        Console.ReadLine();
    }




}
