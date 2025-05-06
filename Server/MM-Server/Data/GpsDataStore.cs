using System;


public class GpsDataStore
{

    public double lat, lon; // Co-ords
    public string name, col; // Name and color for this device/blip
    public long last; // Last updated time

    // Constructor
    public GpsDataStore(GpsDataIn d)
    {
        name = d.name;
        col = d.col;
        lat = d.lat;
        lon = d.lon;
        last = DateTime.Now.Ticks;
    }
    


    // Function to debug to console
    public void Print()
    {
        Console.WriteLine($"Name = {name}, Colour = {col}, Lat = {lat}, Lon = {lon}");
    }


    // Convert Lat/Lon Co-ords to polar Co-ords for ring
    public PolarData PrintPolar(GpsDataStore origin)
    {
        //Deg -> Rad
        double rLon1, rLon2, rLat1, rLat2, R;
        rLat1 = lat * (Math.PI / 180);
        rLat2 = origin.lat * (Math.PI / 180);

        rLon1 = lon * (Math.PI / 180);
        rLon2 = origin.lon * (Math.PI / 180);

        R = 6372795.477598; // Radius of the Earth


        // Distance
        double dA = Math.Sin(rLat1) * Math.Sin(rLat2);
        double dB = Math.Cos(rLat1) * Math.Cos(rLat2);
        double dC = Math.Cos(rLon1 - rLon2);

        double d = R * Math.Acos(dA + dB * dC); // Great Circle Distance (Curved length)



        // Bearing
        double φ2 = lat * Math.PI / 180; //lat
        double λ2 = lon * Math.PI / 180; //lon  

        double φ1 = origin.lat * Math.PI / 180; //lat
        double λ1 = origin.lon * Math.PI / 180; //lon         


        double x = Math.Cos(φ2) * Math.Sin(λ2 - λ1);
        double y = Math.Cos(φ1) * Math.Sin(φ2) -
                   Math.Sin(φ1) * Math.Cos(φ2) * Math.Cos(λ2 - λ1);
        double θ = Math.Atan2(x, y);
        double brng = (θ * 180 / Math.PI + 360) % 360; // in degrees




        // Print distance and bearing (Polar co-ords)
        Console.WriteLine("Calc'd Dist/Bearing:" + d + " " + brng);
        return new PolarData((int)d, (int)brng, col);
    }
}

