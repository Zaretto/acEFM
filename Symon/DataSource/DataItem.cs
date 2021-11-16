using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Symon.DataSource
{
    public class DataItem
    {
        private DataSource ds;
        public string name;
        private string value = "";
        private bool container;
        private bool plot;

        List<DataItem> Children = null;
        DataItem Parent = null;

        public string Name { get => name; set => name = value; }
        public string Value
        {
            get
            {
                return value;
            }
            set
            {
                this.value = value; ds.SetValue(name, value);
            }
        }

        //public bool Plot { get => plot; set => this.plot = value; }
        public DataItem(DataSource ds, String name)
        {
            this.ds = ds;
            this.name = name;
            container = true;
        }

        public bool IsContainer()
        {
            return container;
        }

        public DataItem(DataSource ds, String name, String val)
        {
            this.ds = ds;
            this.name = name;
            this.value = val;
            this.container = false;
        }

        public void SetName(String name)
        {
            this.name = name;
        }

        public void SetValue(string value)
        {
            this.value = value;
        }

        public List<DataItem> GetChildren()
        {
            return Children;
        }

        public void AddChildren(List<DataItem> v)
        {
            Children = v;
            for (int i = 0; i < v.Count(); i++)
                v.ElementAt(i).Parent = this;
        }

        public String GetPath()
        {
            String path = "";
            DataItem p = Parent;

            while (p != null)
            {
                path = p.GetName() + path;
                p = p.Parent;
            }

            return path;
        }
        public String GetName()
        {
            return name;
        }

        public string GetValue()
        {
            return value;
        }

        public bool HasChildren()
        {
            return IsContainer();
        }
        public void Update()
        {
            string tv = ds.CurrentValue(Name);
            if (tv != value) value = tv;
        }
        internal void RequestUpdate()
        {
            ds.UpdateValue(Name);
        }
    }
}

            //// 
            //// chart1
            //// 
            //this.chart1 = new System.Windows.Forms.DataVisualization.Charting.Chart();
            //chartArea1.Name = "ChartArea1";
            //this.chart1.ChartAreas.Add(chartArea1);
            //this.chart1.Location = new System.Drawing.Point(685, 21);
            //this.chart1.Name = "chart1";
            //series1.ChartArea = "ChartArea1";
            //series1.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.FastLine;
            //series1.Name = "Series1";
            //this.chart1.Series.Add(series1);
            //this.chart1.Size = new System.Drawing.Size(404, 94);
            //this.chart1.TabIndex = 8;
            //this.chart1.Text = "chart1";
            //title1.Name = "Title1";
            //title1.Text = "alpha";
            //this.chart1.Titles.Add(title1);
            //((System.ComponentModel.ISupportInitialize)(this.chart1)).BeginInit();
            //this.Controls.Add(this.chart1);
            //((System.ComponentModel.ISupportInitialize)(this.chart1)).EndInit();
