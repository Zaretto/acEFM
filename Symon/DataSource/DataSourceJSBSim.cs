using System;
using System.Collections.Generic;

namespace Symon.DataSource
{
    public class DataSourceJSBSim : DataSource
    {
        private String host = "127.0.0.1"; //TODO: need to override this
        private int nvars = 0;
        private int port = 1137; //TODO: Need to override this.
        private List<string> BatchUpdate;
        private bool BuildingBatch;

        public DataSourceJSBSim(string host, int port) : base(host,port)
        {
        }

        public override List<DataItem> GetItems(String Root)
        {
            var dv = new List<DataItem>();
            String res;
            System.Console.WriteLine("getItems() " + Root);
            //if (sOut == null)
            //{
            //    try
            //    {
            //        openConnection();
            //    }
            //    catch (Exception ex)
            //    {
            //            System.Console.WriteLine("Error: " + this.GetType().Name + " " + ex);
            //        }
            //    }
            if (sender == null)
            {
                res = "fdm/jsbsim/value 0\n";
            }
            else
            {
                Command("HOLD", 1000);
                res = Command("get " + Root, 10000);

                Command("RESUME", 1000);
            }
            res = res.Replace("\r\n", "\n").Replace("\r", "\n").Replace("\n\n", "\n");
            var lines = res.Split('\n');
            //        if (res.length() > 10)
            //        {
            //            for(int x = 0; x < res.length(); x++)
            //            {
            //                System.Console.WriteLine(x+": "+res.charAt(x));
            //                if (x>40) break;
            //            }
            //        }
            //        else
            //            System.Console.WriteLine("Short string "+res+":::\n");

            System.Console.WriteLine("--> " + lines.Length + " lines");
            for (int i = 0; i < lines.Length; i++)
            {
                // skip blanks and prompts
                if (lines[i].Length < 1 || lines[i] == "JSBSim> ")
                {
                    continue;
                }

                //          System.Console.WriteLine(i+" "+lines[i]);

                if (lines[i].EndsWith("/"))
                {
                    // folder
                    DataItem di = new DataItem(this, lines[i]);
                    dv.Add(di);
                    di.AddChildren(GetItems(Root + lines[i]));
                }
                else
                {
                    nvars++;
                    System.Console.WriteLine(nvars + ": " + lines[i]);
                    var v = lines[i].Split(',');
                    if (v.Length == 2)
                    {
                        dv.Add(new DataItem(this, v[0], v[1]));
                    }
                    v = lines[i].Split(' ');
                    if (v.Length == 2)
                    {
                        dv.Add(new DataItem(this, v[0]));
                    }
                }
            }
            return dv;
        }

        internal List<DataItem> LoadFrom(string[] lines)
        {
            var dv = new List<DataItem>();
            System.Console.WriteLine("--> " + lines.Length + " lines");
            foreach(var prop in lines)
            {
                dv.Add(new DataItem(this, prop, ""));
            }
            return dv;
        }

        public override String UpdateValue(String item)
        {
            BatchUpdate.Add(item);
            return CurrentValue(item);
        }
        public override String CurrentValue(String item)
        {
            if (_CurrentValues.ContainsKey(item))
            return _CurrentValues[item];
            return "**";
        }
        Dictionary<string, string> _CurrentValues = new Dictionary<string, string>();

        public override bool BeginBatch()
        {
            BatchUpdate = new List<string>();
            BuildingBatch = true;
            return BuildingBatch;
        }
        public override bool EndBatch()
        {
            String update = "";
            foreach (var item in BatchUpdate)
            {
                update = update + "get " + item + "\r\n";
            }
            string res = Command(update,25);
            BuildingBatch = false;
            BatchUpdate.Clear();
            var lines = res.Split('\n');

            for (int i = 0; i < lines.Length; i++)
            {
                var v = lines[i].Replace("JSBSim> ", "");
                // skip blanks and prompts
                if (string.IsNullOrEmpty(lines[i]))
                {
                    continue;
                }

                //            System.Console.WriteLine("udpate /"+item+" "+res);
                //          System.Console.WriteLine(i+" "+lines[i]);

                if (v.EndsWith("/"))
                {
                    continue;
                }
                else
                {
                    var vals = v.Split('=');
                    if (vals.Length == 2)
                    {
                        _CurrentValues[vals[0].Trim()] = vals[1];
                    }
                }
            }
            return BuildingBatch;
        }

        public override void SetValue(String item, string value)
        {
            String res;
            res = Command("set " + item + " " + value + "\n", 25);
        }

        public override String GetValue(String item)
        {
            String res;
            res = Command("get " + item + "\n", 25);

            var lines = res.Split('\n');

            for (int i = 0; i < lines.Length; i++)
            {
                // skip blanks and prompts
                if (string.IsNullOrEmpty(lines[i]) || lines[i] == "JSBSim> ")
                {
                    continue;
                }

                //            System.Console.WriteLine("udpate /"+item+" "+res);
                //          System.Console.WriteLine(i+" "+lines[i]);

                if (lines[i].EndsWith("/"))
                {
                    continue;
                }
                else
                {
                    var v = lines[i].Split('=');
                    if (v.Length == 2)
                    {
                        _CurrentValues[v[0].Trim()] = v[1];
                        return _CurrentValues[item];
                    }
                }
            }
            return "";
        }

        public override void Hold()
        {
            if (sender == null)
                return;
            System.Console.WriteLine("HOLD");
            System.Console.WriteLine(Command("hold",100));
        }

        public override void OpenConnection()
        {
            OpenConnection(host, port);
        }
        public override void Resume()
        {
            if (sender == null)
                return;

            System.Console.WriteLine("RESUME");
            System.Console.WriteLine(Command("resume",100));
        }
    }
}