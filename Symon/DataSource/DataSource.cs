using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;

namespace Symon.DataSource
{
    public abstract class DataSource
    {
        protected Socket sender = null;

        public DataSource()
        {
        }

        public DataSource(String host, int port)
        {
            try
            {
                OpenConnection(host, port);
               // ReceiveAll();
            }
            catch (Exception ex)
            {
                System.Console.WriteLine("Error: " + this.GetType().Name + " " + ex);
            }
        }
        public virtual bool IsConnected
        {
            get
            {
                if (sender != null)
                    return true;
                return false;
            }
        }

        public virtual void CloseConnection()
        {
            try
            {
                sender.Shutdown(SocketShutdown.Both);
                sender.Close();
                sender = null;
            }
            catch (Exception ex)
            {
                sender = null;
                System.Console.WriteLine("Error: " + this.GetType().Name + " " + ex);
            }
        }

        abstract public List<DataItem> GetItems(String Root);

        abstract public String UpdateValue(String item);
        abstract public String CurrentValue(String item);
        abstract public String GetValue(String item);
        abstract public void SetValue(String item, string value);

        abstract public bool BeginBatch();
        abstract public bool EndBatch();

        abstract public void Hold();

        abstract public void Resume();

        public virtual void Update(DataItem di)
        {
            if (sender == null)
                return;

            String new_value = GetValue(di.GetPath() + di.GetName());
            if (new_value.Length > 0)
            {
                try
                {
                    di.SetValue(new_value);
                }
                catch (FormatException)
                {
                }
            }
        }

        protected String Command(String command, int timeoutMS)
        {
            if (sender != null)
            {
                try
                {
                    if (!command.EndsWith("\r\n"))
                    {
                        command = command.Trim('\n') + "\r\n";
                    }
                    byte[] msg = Encoding.ASCII.GetBytes(command);
                    sender.Send(msg);
                    System.Threading.Thread.Sleep(timeoutMS);
                    // Receive the response from the remote device.
                    string ret = "";
                    ret = ReceiveAll();
                    return ret;
                }
                catch (ArgumentNullException ane)
                {
                    Console.WriteLine("ArgumentNullException : {0}", ane.ToString());
                }
                catch (SocketException se)
                {
                    Console.WriteLine("SocketException : {0}", se.ToString());
                    throw;
                }
                catch (Exception e)
                {
                    Console.WriteLine("Unexpected exception : {0}", e.ToString());
                }
            }
            return "";
        }

        private string ReceiveAll()
        {
            string ret = "";
            int bytesRec = 0;
            sender.ReceiveTimeout = 500;
            do
            {

                byte[] bytes = new byte[32768];
                
                bytesRec = sender.Receive(bytes);
                ret += Encoding.ASCII.GetString(bytes, 0, bytesRec);
                Console.WriteLine("Echoed test = {0}", ret);
            } while (sender.Available > 0);
            return ret;
        }

        abstract public void OpenConnection();

        public void OpenConnection(String host, int port)
        {
            //IPHostEntry ipHostInfo = Dns.GetHostEntry(host);
            //IPAddress ipAddress = ipHostInfo.AddressList[0];

            IPAddress ipAddress = IPAddress.Parse(host);
            IPEndPoint remoteEP = new IPEndPoint(ipAddress, port);

            // Create a TCP/IP  socket.
            sender = new Socket(ipAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp);

            System.Console.WriteLine("Connect " + port);
            try
            {
                sender.Connect(remoteEP);
                sender.ReceiveTimeout = 5000; // 2 minutes in milliseconds
                sender.SendTimeout = 3000;

                Console.WriteLine("Socket connected to {0}", sender.RemoteEndPoint.ToString());
            }
            catch (ArgumentNullException ane)
            {
                Console.WriteLine("ArgumentNullException : {0}", ane.ToString());
            }
            catch (SocketException se)
            {
                Console.WriteLine("SocketException : {0}", se.ToString());
            }
            catch (Exception e)
            {
                Console.WriteLine("Unexpected exception : {0}", e.ToString());
            }
        }
    }
}