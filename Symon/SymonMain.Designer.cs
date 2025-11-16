namespace Symon
{
    partial class SymonMain
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            this.timerMonitor = new System.Windows.Forms.Timer(this.components);
            this.panel1 = new System.Windows.Forms.Panel();
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.groupBoxBusy = new System.Windows.Forms.GroupBox();
            this.progressBar1 = new System.Windows.Forms.ProgressBar();
            this.bbLabel = new System.Windows.Forms.Label();
            this.dataGridView1 = new System.Windows.Forms.DataGridView();
            this.panel2 = new System.Windows.Forms.Panel();
            this.btnPaste = new System.Windows.Forms.Button();
            this.btnSaveSelected = new System.Windows.Forms.Button();
            this.btnLoadSelected = new System.Windows.Forms.Button();
            this.btnPlot = new System.Windows.Forms.Button();
            this.buttonClear = new System.Windows.Forms.Button();
            this.cbSaveIndex = new System.Windows.Forms.ComboBox();
            this.addRowButton = new System.Windows.Forms.Button();
            this.buttonDisconnect = new System.Windows.Forms.Button();
            this.buttonMonitor = new System.Windows.Forms.Button();
            this.buttonReload = new System.Windows.Forms.Button();
            this.statusStrip1 = new System.Windows.Forms.StatusStrip();
            this.toolStripStatusLabel1 = new System.Windows.Forms.ToolStripStatusLabel();
            this.btnCopy = new System.Windows.Forms.Button();
            this.panel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            this.groupBoxBusy.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.dataGridView1)).BeginInit();
            this.panel2.SuspendLayout();
            this.statusStrip1.SuspendLayout();
            this.SuspendLayout();
            // 
            // timerMonitor
            // 
            this.timerMonitor.Tick += new System.EventHandler(this.timerMonitor_Tick);
            // 
            // panel1
            // 
            this.panel1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.panel1.BackColor = System.Drawing.SystemColors.ControlLight;
            this.panel1.Controls.Add(this.splitContainer1);
            this.panel1.Controls.Add(this.panel2);
            this.panel1.Controls.Add(this.buttonReload);
            this.panel1.Location = new System.Drawing.Point(34, 21);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(771, 886);
            this.panel1.TabIndex = 8;
            // 
            // splitContainer1
            // 
            this.splitContainer1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.splitContainer1.Location = new System.Drawing.Point(6, 3);
            this.splitContainer1.Name = "splitContainer1";
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.groupBoxBusy);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.dataGridView1);
            this.splitContainer1.Size = new System.Drawing.Size(695, 828);
            this.splitContainer1.SplitterDistance = 234;
            this.splitContainer1.TabIndex = 19;
            // 
            // groupBoxBusy
            // 
            this.groupBoxBusy.Controls.Add(this.progressBar1);
            this.groupBoxBusy.Controls.Add(this.bbLabel);
            this.groupBoxBusy.Location = new System.Drawing.Point(23, 30);
            this.groupBoxBusy.Name = "groupBoxBusy";
            this.groupBoxBusy.Size = new System.Drawing.Size(651, 359);
            this.groupBoxBusy.TabIndex = 18;
            this.groupBoxBusy.TabStop = false;
            this.groupBoxBusy.Text = "Application is busy";
            // 
            // progressBar1
            // 
            this.progressBar1.Location = new System.Drawing.Point(85, 109);
            this.progressBar1.Name = "progressBar1";
            this.progressBar1.Size = new System.Drawing.Size(239, 22);
            this.progressBar1.Style = System.Windows.Forms.ProgressBarStyle.Continuous;
            this.progressBar1.TabIndex = 1;
            // 
            // bbLabel
            // 
            this.bbLabel.AutoSize = true;
            this.bbLabel.Location = new System.Drawing.Point(7, 20);
            this.bbLabel.Name = "bbLabel";
            this.bbLabel.Size = new System.Drawing.Size(121, 13);
            this.bbLabel.TabIndex = 0;
            this.bbLabel.Text = "Processing, please wait.";
            // 
            // dataGridView1
            // 
            this.dataGridView1.AllowUserToOrderColumns = true;
            this.dataGridView1.AllowUserToResizeRows = false;
            this.dataGridView1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.dataGridView1.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.dataGridView1.Location = new System.Drawing.Point(3, 3);
            this.dataGridView1.Name = "dataGridView1";
            this.dataGridView1.Size = new System.Drawing.Size(521, 822);
            this.dataGridView1.TabIndex = 16;
            // 
            // panel2
            // 
            this.panel2.Controls.Add(this.btnCopy);
            this.panel2.Controls.Add(this.btnPaste);
            this.panel2.Controls.Add(this.btnSaveSelected);
            this.panel2.Controls.Add(this.btnLoadSelected);
            this.panel2.Controls.Add(this.btnPlot);
            this.panel2.Controls.Add(this.buttonClear);
            this.panel2.Controls.Add(this.cbSaveIndex);
            this.panel2.Controls.Add(this.addRowButton);
            this.panel2.Controls.Add(this.buttonDisconnect);
            this.panel2.Controls.Add(this.buttonMonitor);
            this.panel2.Location = new System.Drawing.Point(3, 837);
            this.panel2.Name = "panel2";
            this.panel2.Size = new System.Drawing.Size(1049, 31);
            this.panel2.TabIndex = 17;
            // 
            // btnPaste
            // 
            this.btnPaste.Location = new System.Drawing.Point(12, 3);
            this.btnPaste.Name = "btnPaste";
            this.btnPaste.Size = new System.Drawing.Size(47, 23);
            this.btnPaste.TabIndex = 40;
            this.btnPaste.Text = "paste";
            this.btnPaste.UseVisualStyleBackColor = true;
            this.btnPaste.Click += new System.EventHandler(this.btnPaste_Click);
            // 
            // btnSaveSelected
            // 
            this.btnSaveSelected.Location = new System.Drawing.Point(444, 4);
            this.btnSaveSelected.Name = "btnSaveSelected";
            this.btnSaveSelected.Size = new System.Drawing.Size(75, 26);
            this.btnSaveSelected.TabIndex = 39;
            this.btnSaveSelected.Text = "save sel";
            this.btnSaveSelected.UseVisualStyleBackColor = true;
            this.btnSaveSelected.Click += new System.EventHandler(this.btnSaveSelected_Click);
            // 
            // btnLoadSelected
            // 
            this.btnLoadSelected.Location = new System.Drawing.Point(365, 4);
            this.btnLoadSelected.Name = "btnLoadSelected";
            this.btnLoadSelected.Size = new System.Drawing.Size(73, 26);
            this.btnLoadSelected.TabIndex = 38;
            this.btnLoadSelected.Text = "load sel";
            this.btnLoadSelected.UseVisualStyleBackColor = true;
            this.btnLoadSelected.Click += new System.EventHandler(this.btnLoadSelected_Click);
            // 
            // btnPlot
            // 
            this.btnPlot.Location = new System.Drawing.Point(582, 2);
            this.btnPlot.Name = "btnPlot";
            this.btnPlot.Size = new System.Drawing.Size(51, 26);
            this.btnPlot.TabIndex = 33;
            this.btnPlot.Text = "plot";
            this.btnPlot.UseVisualStyleBackColor = true;
            this.btnPlot.Click += new System.EventHandler(this.btnPlot_Click);
            // 
            // buttonClear
            // 
            this.buttonClear.Location = new System.Drawing.Point(525, 3);
            this.buttonClear.Name = "buttonClear";
            this.buttonClear.Size = new System.Drawing.Size(51, 26);
            this.buttonClear.TabIndex = 33;
            this.buttonClear.Text = "clear";
            this.buttonClear.UseVisualStyleBackColor = true;
            this.buttonClear.Click += new System.EventHandler(this.buttonClear_Click);
            // 
            // cbSaveIndex
            // 
            this.cbSaveIndex.FormattingEnabled = true;
            this.cbSaveIndex.Items.AddRange(new object[] {
            "<default>",
            "1",
            "2",
            "3",
            "4",
            "5",
            "6",
            "7",
            "8",
            "9",
            "10",
            "11",
            "12",
            "13",
            "14",
            "15",
            "16"});
            this.cbSaveIndex.Location = new System.Drawing.Point(266, 7);
            this.cbSaveIndex.Name = "cbSaveIndex";
            this.cbSaveIndex.Size = new System.Drawing.Size(93, 21);
            this.cbSaveIndex.TabIndex = 37;
            this.cbSaveIndex.SelectedIndexChanged += new System.EventHandler(this.cbSaveIndex_SelectedIndexChanged);
            this.cbSaveIndex.KeyDown += new System.Windows.Forms.KeyEventHandler(this.cbSaveIndex_KeyDown);
            // 
            // addRowButton
            // 
            this.addRowButton.Location = new System.Drawing.Point(227, 4);
            this.addRowButton.Name = "addRowButton";
            this.addRowButton.Size = new System.Drawing.Size(35, 26);
            this.addRowButton.TabIndex = 36;
            this.addRowButton.Text = "+";
            this.addRowButton.UseVisualStyleBackColor = true;
            this.addRowButton.Click += new System.EventHandler(this.addRowButton_Click);
            // 
            // buttonDisconnect
            // 
            this.buttonDisconnect.Location = new System.Drawing.Point(105, 2);
            this.buttonDisconnect.Name = "buttonDisconnect";
            this.buttonDisconnect.Size = new System.Drawing.Size(71, 26);
            this.buttonDisconnect.TabIndex = 34;
            this.buttonDisconnect.Text = "disconnect";
            this.buttonDisconnect.UseVisualStyleBackColor = true;
            this.buttonDisconnect.Click += new System.EventHandler(this.buttonDisconnect_Click);
            // 
            // buttonMonitor
            // 
            this.buttonMonitor.Location = new System.Drawing.Point(170, 2);
            this.buttonMonitor.Name = "buttonMonitor";
            this.buttonMonitor.Size = new System.Drawing.Size(51, 26);
            this.buttonMonitor.TabIndex = 32;
            this.buttonMonitor.Text = "Monitor";
            this.buttonMonitor.UseVisualStyleBackColor = true;
            this.buttonMonitor.Click += new System.EventHandler(this.buttonMonitor_Click);
            // 
            // buttonReload
            // 
            this.buttonReload.Location = new System.Drawing.Point(652, 860);
            this.buttonReload.Name = "buttonReload";
            this.buttonReload.Size = new System.Drawing.Size(51, 26);
            this.buttonReload.TabIndex = 30;
            this.buttonReload.Text = "get jsb";
            this.buttonReload.UseVisualStyleBackColor = true;
            this.buttonReload.Click += new System.EventHandler(this.buttonReload_Click);
            // 
            // statusStrip1
            // 
            this.statusStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.toolStripStatusLabel1});
            this.statusStrip1.Location = new System.Drawing.Point(0, 897);
            this.statusStrip1.Name = "statusStrip1";
            this.statusStrip1.Size = new System.Drawing.Size(817, 22);
            this.statusStrip1.TabIndex = 9;
            this.statusStrip1.Text = "statusStrip1";
            // 
            // toolStripStatusLabel1
            // 
            this.toolStripStatusLabel1.Name = "toolStripStatusLabel1";
            this.toolStripStatusLabel1.Size = new System.Drawing.Size(118, 17);
            this.toolStripStatusLabel1.Text = "toolStripStatusLabel1";
            this.toolStripStatusLabel1.Click += new System.EventHandler(this.toolStripStatusLabel1_Click);
            // 
            // btnCopy
            // 
            this.btnCopy.Location = new System.Drawing.Point(65, 3);
            this.btnCopy.Name = "btnCopy";
            this.btnCopy.Size = new System.Drawing.Size(47, 23);
            this.btnCopy.TabIndex = 40;
            this.btnCopy.Text = "copy";
            this.btnCopy.UseVisualStyleBackColor = true;
            this.btnCopy.Click += new System.EventHandler(this.btnCopy_Click);
            // 
            // SymonMain
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(817, 919);
            this.Controls.Add(this.statusStrip1);
            this.Controls.Add(this.panel1);
            this.Name = "SymonMain";
            this.Text = "DCS JSBSim Symon";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.SymonMain_FormClosing);
            this.panel1.ResumeLayout(false);
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.groupBoxBusy.ResumeLayout(false);
            this.groupBoxBusy.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.dataGridView1)).EndInit();
            this.panel2.ResumeLayout(false);
            this.statusStrip1.ResumeLayout(false);
            this.statusStrip1.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion
        private System.Windows.Forms.Timer timerMonitor;
        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.Panel panel2;
        private System.Windows.Forms.Button buttonDisconnect;
        private System.Windows.Forms.Button buttonReload;
        private System.Windows.Forms.Button buttonClear;
        private System.Windows.Forms.Button buttonMonitor;
        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.DataGridView dataGridView1;
        private System.Windows.Forms.GroupBox groupBoxBusy;
        private System.Windows.Forms.Label bbLabel;
        private System.Windows.Forms.ProgressBar progressBar1;
        private System.Windows.Forms.StatusStrip statusStrip1;
        private System.Windows.Forms.ToolStripStatusLabel toolStripStatusLabel1;
        private System.Windows.Forms.Button addRowButton;
        private System.Windows.Forms.ComboBox cbSaveIndex;
        private System.Windows.Forms.Button btnSaveSelected;
        private System.Windows.Forms.Button btnLoadSelected;
        private System.Windows.Forms.Button btnPaste;
        private System.Windows.Forms.Button btnPlot;
        private System.Windows.Forms.Button btnCopy;
    }
}

