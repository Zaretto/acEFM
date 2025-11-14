using Be.Timvw.Framework.ComponentModel;
using Microsoft.Win32;
using Symon.DataSource;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Net.Sockets;
using System.Reflection.Emit;
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
        }
        protected async override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            await PerformOperation("Loading properties", () =>
            {
                LoadProps();
            });

        }
        void SaveSelected(string idx)
        {
            if (idx == "<default>") idx = "";
            if (monitoredVariables.Count > 0)
            {
                var vals = String.Join(",", monitoredVariables.OfType<DataItem>().Select(xx => xx.Name));
                Registry.SetValue(@"HKEY_CURRENT_USER\SOFTWARE\Symon\Vars", "Selected" + idx, vals);
            }

        }
        void LoadSelected(string idx)
        {
            if (idx == "<default>") idx = "";
            var selected_l = (string)Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Symon\Vars", "Selected" + idx, "");

            string[] selected = null;
            if (!string.IsNullOrEmpty(selected_l))
                selected = selected_l.Split(',');
            monitoredVariables.Clear();
            if (selected != null)
            {
                foreach (var s in selected.Distinct())
                {
                    monitoredVariables.Add(new DataItem(jsbds, s));
                }
            }
        }

        void SaveProps()
        {
            if (Items.Any())
            {
                string vals = String.Join(",", Items.Select(xx => xx.Name));
                Registry.SetValue(@"HKEY_CURRENT_USER\SOFTWARE\Symon\Vars", "Properties", vals);
            }
            SaveSelected("");
        }
        void LoadProps()
        {
            var props = (string)Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Symon\Vars", "Properties", "");

            //var index = dataGridView1.Rows.Add();
            //dataGridView1.Rows[index].Cells["Property"].Value = treeView1.SelectedNode.Text;
            //dataGridView1.Rows[index].Cells["Value"].Value = jsbds.GetValue(treeView1.SelectedNode.Text);
            //this.dataGridView1.DataSource = null;
            delegateToMainThread(() =>
            {
                try
                {
                    if (!string.IsNullOrEmpty(props))
                    {
                        Items = jsbds.LoadFrom(props.Split(','));
                        TreeNode parentNode;
                        foreach (var di in Items.OrderBy(xx => xx.GetName()))
                        {
                            parentNode = treeView1.Nodes.Add(di.GetName());
                            parentNode.Tag = di;
                        }
                        treeView1.ExpandAll();
                    }
                    LoadSelected("");
                    ClearError();
                }
                catch (SocketException ex)
                {
                    ShowError("Socket exception ", ex.Message);
                }

            });
            //this.dataGridView1.DataSource = dataviewsource;
            //dataviewsource.ResetBindings(false);
            //dataviewsource.EndEdit();
            //dataGridView1.EndEdit();

        }
        private void LoadProperties()
        {
            try
            {
                TreeNode parentNode;
                Items = jsbds.GetItems("/");
                delegateToMainThread(() =>
                {
                    treeView1.Nodes.Clear();
                    foreach (var di in Items.OrderBy(xx => xx.GetName()))
                    {
                        parentNode = treeView1.Nodes.Add(di.GetName());
                        parentNode.Tag = di;
                    }
                    treeView1.ExpandAll();
                });
                ClearError();

            }
            catch (SocketException ex)
            {
                if (ex.Message.Contains("failed to respond"))
                    ShowError("Error getting data", "Timeout");
                else
                    ShowError("Socket exception ", ex.Message);
            }
            catch (InvalidOperationException ex)
            {
                ShowError("LoadProperties From JsbSim: ERROR ", ex.Message);
            }

        }


        private async void buttonLoad_Click(object sender, EventArgs e)
        {
            await PerformOperation("Loading properties from simulation", () =>
            {
                LoadProperties();
                LoadProps();
            });
        }
        void delegateToMainThread(Action a)
        {
            this.BeginInvoke(new Action(() =>
            {
                // Perform your operations here

                a();
            }));
        }
        private void SetControlsEnabled(bool enabled)
        {
            foreach (Control control in this.Controls)
            {
                control.Enabled = enabled;
            }
        }

        private async Task PerformOperation(string description, Action value)
        {
            try
            {
                // Disable controls
                SetControlsEnabled(false);
                groupBoxBusy.Visible = true;
                bbLabel.Text = description;
                await Task.Run(() =>
                {
                    value();
                });
            }
            finally
            {
                // Enable controls
                SetControlsEnabled(true);
                groupBoxBusy.Visible = false;
            }
        }

        private void buttonUpdate_Click(object sender, EventArgs e)
        {
            UpdateValues();
        }

        private void UpdateValues()
        {
            try
            {
                jsbds.BeginBatch();
                foreach (var di in monitoredVariables.OfType<DataItem>())
                    di.RequestUpdate();
                jsbds.EndBatch();
                foreach (var di in monitoredVariables.OfType<DataItem>())
                    di.Update();
                //foreach (var di in monitoredVariables.OfType<DataItem>())
                //    di.GetValue();
                //                dataviewsource.ResetBindings(false);
                ClearError();

            }
            catch (SocketException ex)
            {
                ShowError("Socket exception ", ex.Message);
                timerMonitor.Stop();

                buttonDisconnect_Click(null, null);
            }
        }
        SortableBindingList<DataItem> monitoredVariables = new SortableBindingList<DataItem>();
        //            BindingList<DataItem> monitoredVariables = new BindingList<DataItem>();
        public void RemoveDuplicates()
        {
            var seen = new HashSet<string>();  // Change type as needed
            var toRemove = new List<DataItem>();

            foreach (var item in monitoredVariables)
            {
                if (!seen.Add(item.Name))  // Returns false if already exists
                {
                    toRemove.Add(item);
                }
            }

            foreach (var item in toRemove)
            {
                monitoredVariables.Remove(item);
            }
        }
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
            try
            {
                if (Monitoring)
                {
                    Monitoring = false;
                    buttonMonitor.Text = "Monitoring";
                    timerMonitor.Stop();
                }
                else
                {
                    RemoveDuplicates();
                    if (!jsbds.IsConnected)
                    {
                        buttonDisconnect_Click(sender, e);
                    }
                    Monitoring = true;
                    buttonMonitor.Text = "Stop Monitoring";
                    timerMonitor.Interval = 500;
                    timerMonitor.Start();
                }
            }
            catch (SocketException)
            {

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
            try
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
                ClearError();

            }
            catch (SocketException ex)
            {
                ShowError("Socket exception ", ex.Message);
            }
        }
        private async void buttonReload_Click(object sender, EventArgs e)
        {
            //            monitoredVariables.Clear();
            await PerformOperation("Reloading properties", () =>
            {
                LoadProperties();
            });
        }

        private void buttonSave_Click(object sender, EventArgs e)
        {
            SaveProps();
        }

        private void toolStripStatusLabel1_Click(object sender, EventArgs e)
        {

        }
        void ShowError(string title, string message)
        {
            toolStripStatusLabel1.Text =
                DateTime.Now.ToString("HH:mm:ss.ff")
                + title
                + message;
        }
        void ClearError()
        {
            //if (toolStripStatusLabel1.Text != "")
            //    toolStripStatusLabel1.Text = "";
        }

        private void addRowButton_Click(object sender, EventArgs e)
        {
            var ni = new DataItem(jsbds, "");
            monitoredVariables.Add(ni);
        }

        private void button1_Click(object sender, EventArgs e)
        {



        }

        private void btnLoadSelected_Click(object sender, EventArgs e)
        {
            LoadSelected(cbSaveIndex.Text);

        }

        private void btnSaveSelected_Click(object sender, EventArgs e)
        {
            SaveSelected(cbSaveIndex.Text);

        }

        private void cbShowJSBTree_CheckedChanged(object sender, EventArgs e)
        {
            treeView1.Visible = cbShowJSBTree.Checked;
        }

        private void btnPaste_Click(object sender, EventArgs e)
        {
            // Get the clipboard data
            if (Clipboard.ContainsText())
            {
                string clipboardText = Clipboard.GetText();
                string[] lines = clipboardText.Split(new[] { "\r\n", "\r", "\n" }, StringSplitOptions.None);

                foreach (var line in lines.Where(xx => xx.Contains("/")))
                {
                    var property = line.Trim();
                    if (!monitoredVariables.Any(xx => xx.Name == property))
                    {
                        var ni = new DataItem(jsbds, line.Trim());
                        monitoredVariables.Add(ni);
                    }
                }
            }
        }
    }
}
