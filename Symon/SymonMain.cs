using Be.Timvw.Framework.ComponentModel;
using Microsoft.Win32;
using Symon.DataSource;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using System.Reflection.Emit;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Xml.Linq;

namespace Symon
{
    public partial class SymonMain : Form
    {
        private DataSourceJSBSim jsbds;
        private BindingSource dataviewsource;
        private readonly StripChartContainer stripCharts = new StripChartContainer();
        private readonly string RegistrySavePath = @"Software\Symon\Vars";
        private string RegistrySaveKeyPrefix = "Selected";

        public SymonMain()
        {
            InitializeComponent();
            jsbds = new DataSourceJSBSim("127.0.0.1", 1137);

            dataviewsource = new BindingSource();
            dataviewsource.DataSource = monitoredVariables;
            dataGridView1.DataSource = dataviewsource;
            ReloadSaveList();
            // now add the strip charts on the left;
            stripCharts.Dock = DockStyle.Fill;
            splitContainer1.Panel1.Controls.Add(stripCharts);
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
            try
            {
                if (idx == "<default>") idx = "";
                if (monitoredVariables.Count > 0)
                {
                    var vals = String.Join(",", monitoredVariables.OfType<DataItem>().Select(xx => xx.Name + (stripCharts.IsPlotted(xx.Name) ? "~" : "")));
                    using (RegistryKey baseKey = Registry.CurrentUser.OpenSubKey(RegistrySavePath, writable: true))
                    {
                        if (baseKey != null)
                        {
                            baseKey.SetValue(RegistrySaveKeyPrefix + idx, vals);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message);

            }
        }
        private void ReloadSaveList()
        {
            var newList = new List<string>();
            
            using (RegistryKey baseKey = Registry.CurrentUser.OpenSubKey(RegistrySavePath))
            {
                if (baseKey != null)
                {
                    foreach (string subKeyName in baseKey.GetValueNames().Where(xx => xx.StartsWith(RegistrySaveKeyPrefix)))
                    {
                        newList.Add(subKeyName.Substring(RegistrySaveKeyPrefix.Length));
                    }
                }
                else
                {
                    Console.WriteLine("Registry key not found.");
                }
            }
            cbSaveIndex.Items.Clear();
            cbSaveIndex.Items.AddRange(newList.ToArray());
        }
        void ClearSelected(string idx)
        {
            // don't need to confirm because easy enough to by saving again.
            try
            {
                using (RegistryKey key = Registry.CurrentUser.OpenSubKey(RegistrySavePath, true))
                {
                    if (key != null)
                    {
                        key.DeleteValue("Selected" + idx);
                        ReloadSaveList();
                        cbSaveIndex.Text = "";
                    }
                }
            }
            catch (Exception ex)
            {
                ShowError("Clear: ", ex.Message);
            }
        }

        void LoadSelected(string idx)
        {
            if (string.IsNullOrEmpty(idx) || idx == "<default>")
                return;

            using (RegistryKey key = Registry.CurrentUser.OpenSubKey(RegistrySavePath))
            {
                if (key != null)
                {
                    var selected_l = key.GetValue("Selected" + idx, "") as string;
                    string[] selected = null;
                    if (!string.IsNullOrEmpty(selected_l))
                        selected = selected_l.Split(',');
                    monitoredVariables.Clear();
                    stripCharts.Clear();
                    if (selected != null)
                    {
                        foreach (var s in selected.Distinct())
                        {
                            var property = s.Trim('~');
                            var di = NewDataItem(property);
                            if (s.EndsWith("~"))
                                stripCharts.AddDataItem(di);
                        }
                    }
                    StartMonitoring();
                }
            }

        }

        void SaveProps()
        {
            if (Items.Any())
            {
                string vals = String.Join(",", Items.Select(xx => xx.Name));
                using (RegistryKey key = Registry.CurrentUser.OpenSubKey(RegistrySavePath, writable: true))
                {
                    if (key != null)
                        key.SetValue("Properties", vals);
                    SaveSelected("");
                }
            }
        }

        void LoadProps()
        {
            using (RegistryKey key = Registry.CurrentUser.OpenSubKey(RegistrySaveKeyPrefix))
            {
                if (key != null)
                {
                    var props = (string)key.GetValue("Properties", "");
                }
            }
        }
        private void LoadProperties()
        {
            try
            {
                //TreeNode parentNode;
                Items = jsbds.GetItems("/");
                //delegateToMainThread(() =>
                //{
                //    treeView1.Nodes.Clear();
                //    foreach (var di in Items.OrderBy(xx => xx.GetName()))
                //    {
                //        parentNode = treeView1.Nodes.Add(di.GetName());
                //        parentNode.Tag = di;
                //    }
                //    treeView1.ExpandAll();
                //});
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

                stripCharts.Invalidate();
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
            //monitoredVariables.Add(treeView1.SelectedNode.Tag as DataItem);
        }
        private void StartMonitoring()
        {
            if (Monitoring)
                return;
            if (!jsbds.IsConnected)
            {
                jsbds.CloseConnection();
            }
            jsbds.OpenConnection();
            Monitoring = true;
            buttonMonitor.Text = "Stop Monitoring";
            timerMonitor.Interval = 500;
            timerMonitor.Start();

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
            stripCharts.Clear();
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
            NewDataItem("");
        }

        private void button1_Click(object sender, EventArgs e)
        {



        }

        private void btnDeleteSel_Click(object sender, EventArgs e)
        {
            ClearSelected(cbSaveIndex.Text);

        }

        private void btnLoadSelected_Click(object sender, EventArgs e)
        {
            LoadSelected(cbSaveIndex.Text);

        }

        private void btnSaveSelected_Click(object sender, EventArgs e)
        {
            SaveSelected(cbSaveIndex.Text);
            ReloadSaveList();
        }


        private void cbShowJSBTree_CheckedChanged(object sender, EventArgs e)
        {
            //treeView1.Visible = cbShowJSBTree.Checked;
        }
        static string ExtractPropertyName(string input)
        {
            input = input.Trim();
            try
            {
                // Try to parse as XML
                var element = XElement.Parse(input);

                if (element.Name == "property")
                {
                    // Property name is inside the element text
                    return element.Value.Trim();
                }
                else if (element.Name == "fcs_function" || element.Name == "function")
                {
                    // Property name is in the "property" attribute
                    return element.Attribute("name")?.Value.Trim();
                }
                if (!string.IsNullOrEmpty(element.Value) && element.Value.Contains("/"))
                    return element.Value.Trim();
            }
            catch
            {
            }
            // Case 2: Handle malformed/unclosed tags with regex
            // Regex explanation:
            // (?:<property[^>]*>)? → optional opening <property> tag (with or without attributes)
            // ([^<]+) → capture the property name (everything until a '<')
            // (?:</property>)? → optional closing </property> tag
            if (input.StartsWith("<"))
            {
                // Look for property="..."
                var matchProperty = Regex.Match(input, @"\bproperty\s*=\s*""([^""]+)""");
                if (matchProperty.Success)
                    return matchProperty.Groups[1].Value.Trim();

                // Look for name="..."
                var matchName = Regex.Match(input, @"\bname\s*=\s*""([^""]+)""");
                if (matchName.Success)
                    return matchName.Groups[1].Value.Trim();
            }
            else
            {
                var parts = input.Split(' ');
                if (parts[0].Contains("/"))
                    return parts[0];
            }
            return null;
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
                    var property = ExtractPropertyName(line);
                    if (!string.IsNullOrEmpty(property))
                    {
                        property = property.TrimStart('-');
                        if (!monitoredVariables.Any(xx => xx.Name == property))
                        {
                            NewDataItem(property);
                        }
                    }
                }
            }
        }
        protected DataItem NewDataItem(string property)
        {
            if (property.Contains("\r"))
                property = property.Split('\r')[0];
            if (property.Contains("\n"))
                property = property.Split('\n')[0];

            var di = new DataItem(jsbds, property);
            monitoredVariables.Add(di);
            dataGridView1.Columns[0].Width = 300;
            dataGridView1.Columns[1].Width = 100;
            return di;

        }
        private void btnPlot_Click(object sender, EventArgs e)
        {
            var selectedItems = new List<DataItem>();

            foreach (DataGridViewRow row in dataGridView1.SelectedRows)
            {
                if (row.DataBoundItem is DataItem item)
                {
                    stripCharts.AddDataItem(item);
                }
            }
        }

        private void cbSaveIndex_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (!monitoredVariables.Any())
                LoadSelected(cbSaveIndex.Text);

        }

        private void cbSaveIndex_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter)
            {
                var typedText = cbSaveIndex.Text;
                LoadSelected(typedText);
                e.Handled = true; // optional: prevent ding sound
            }
        }

        private void btnCopy_Click(object sender, EventArgs e)
        {
            string format = "<property value=\"{0}\">{1}</property>";
            if ((Control.ModifierKeys & Keys.Control) == Keys.Control)
                format = "<property caption=\"{2}\">{1}</property>";
            var vars = new List<string>();
            foreach (DataGridViewRow row in dataGridView1.SelectedRows)
            {
                if (row.DataBoundItem is DataItem item)
                {
                    vars.Add(string.Format(format, item.GetDoubleValue(), item.Name, item.GetShortName()));
                }
            }
            if (vars.Count > 0)
            {
                vars.Add("");
                Clipboard.SetText(string.Join("\r\n", vars));
            }
        }

    }
}
