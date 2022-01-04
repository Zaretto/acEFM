using Be.Timvw.Framework.ComponentModel;
using Microsoft.Win32;
using Symon.DataSource;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Symon
{
    public partial class SymonMain : Form
    {
        private DataSourceJSBSim jsbds;
        private BindingSource dataviewsource;

        public SymonMain()
        {
            InitializeComponent();
            jsbds = new DataSourceJSBSim("127.0.0.1", 1137);
            //dataGridView1.Columns.Add("Property", "Property");
            //dataGridView1.Columns.Add("Value", "Value");
            //this.dataGridView1.AutoGenerateColumns = true;
            //this.dataGridView1.DataSource = monitoredVariables;
            dataviewsource = new BindingSource();
            dataviewsource.DataSource = monitoredVariables;
            dataGridView1.DataSource = dataviewsource;
            LoadProps();
      
        }
        void SaveProps()
        {
            if (Items.Any())
            {
                string vals = String.Join(",", Items.Select(xx => xx.Name));
                Registry.SetValue(@"HKEY_CURRENT_USER\SOFTWARE\Symon\Vars", "Properties", vals);
            }
            if (monitoredVariables.Count > 0)
            {
                var vals = String.Join(",", monitoredVariables.OfType<DataItem>().Select(xx => xx.Name));
                Registry.SetValue(@"HKEY_CURRENT_USER\SOFTWARE\Symon\Vars", "Selected", vals);
            }
        }
        void LoadProps()
        {
            var props = (string)Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Symon\Vars", "Properties", "");
            var selected_l = (string)Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Symon\Vars", "Selected", "");
            string[] selected = null;
            if (!string.IsNullOrEmpty(selected_l))
                selected = selected_l.Split(',');
            //var index = dataGridView1.Rows.Add();
            //dataGridView1.Rows[index].Cells["Property"].Value = treeView1.SelectedNode.Text;
            //dataGridView1.Rows[index].Cells["Value"].Value = jsbds.GetValue(treeView1.SelectedNode.Text);
            //this.dataGridView1.DataSource = null;

            if (!string.IsNullOrEmpty(props))
            {
                Items = jsbds.LoadFrom(props.Split(','));
                TreeNode parentNode;
                foreach (var di in Items.OrderBy(xx => xx.GetName()))
                {
                    parentNode = treeView1.Nodes.Add(di.GetName());
                    if (selected .Contains(di.Name))
                        monitoredVariables.Add(di);
                    parentNode.Tag = di;
                }
                treeView1.ExpandAll();
            }
            //this.dataGridView1.DataSource = dataviewsource;
            //dataviewsource.ResetBindings(false);
            //dataviewsource.EndEdit();
            //dataGridView1.EndEdit();

        }
        private void LoadProperties()
        {
            TreeNode parentNode;
            Items = jsbds.GetItems("/");
            treeView1.Nodes.Clear();
            foreach (var di in Items.OrderBy(xx=>xx.GetName()))
            {
                parentNode = treeView1.Nodes.Add(di.GetName());
                parentNode.Tag = di;
            }
            treeView1.ExpandAll();
        }
     
        
        private void buttonLoad_Click(object sender, EventArgs e)
        {
            LoadProperties();
            LoadProps();
        }

        private void buttonUpdate_Click(object sender, EventArgs e)
        {
            UpdateValues();
        }

        private void UpdateValues()
        {
            jsbds.BeginBatch();
            foreach (var di in monitoredVariables.OfType<DataItem>())
                di.RequestUpdate();
            jsbds.EndBatch();
            foreach (var di in monitoredVariables.OfType<DataItem>())
                di.Update();
            //foreach (var di in monitoredVariables.OfType<DataItem>())
            //    di.GetValue();
            dataviewsource.ResetBindings(false);
        }
        SortableBindingList<DataItem> monitoredVariables = new SortableBindingList<DataItem>();
//            BindingList<DataItem> monitoredVariables = new BindingList<DataItem>();

        public bool Monitoring { get; private set; }
        public List<DataItem> Items { get; private set; }

        private void treeView1_DoubleClick(object sender, EventArgs e)
        {
            //this.dataGridView1.DataSource = null;
            monitoredVariables.Add(treeView1.SelectedNode.Tag as DataItem);
            //var index = dataGridView1.Rows.Add();
            //dataGridView1.Rows[index].Cells["Property"].Value = treeView1.SelectedNode.Text;
            //dataGridView1.Rows[index].Cells["Value"].Value = jsbds.GetValue(treeView1.SelectedNode.Text);
            //this.dataGridView1.DataSource = null;
            //dataviewsource.ResetBindings(false);
            //this.dataGridView1.DataSource = dataviewsource;
            //dataviewsource.ResetBindings(false);
            //dataviewsource.EndEdit();
            //dataGridView1.EndEdit();
        }

        private void buttonMonitor_Click(object sender, EventArgs e)
        {
            if (Monitoring)
            {
                Monitoring = false;
                buttonMonitor.Text = "Monitoring";
                timerMonitor.Stop();
            }
            else
            {
                Monitoring = true;
                buttonMonitor.Text = "Stop Monitoring";
                timerMonitor.Interval = 500;
                timerMonitor.Start();
            }
        }

        private void timerMonitor_Tick(object sender, EventArgs e)
        {
            UpdateValues();
        }

        private void SymonMain_FormClosing(object sender, FormClosingEventArgs e)
        {
        }

        private void buttonClear_Click(object sender, EventArgs e)
        {
            Registry.SetValue(@"HKEY_CURRENT_USER\SOFTWARE\Symon\Vars", "Properties", "");

            monitoredVariables.Clear();
            dataGridView1.Invalidate();
        }

        private void buttonDisconnect_Click(object sender, EventArgs e)
        {
            if (jsbds.IsConnected)
            {
                if (Monitoring)
                {
                    buttonMonitor_Click(sender, e);
                }
                jsbds.CloseConnection();
                buttonDisconnect.Text = "connect";
            }
            else
            {
                jsbds.OpenConnection();
                buttonDisconnect.Text = "disconnect";
            }
        }

        private void buttonReload_Click(object sender, EventArgs e)
        {
//            monitoredVariables.Clear();
            LoadProperties();
        }

        private void buttonSave_Click(object sender, EventArgs e)
        {
            SaveProps();
        }
    }
}
