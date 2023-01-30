namespace Serial_Logger
{
    partial class Form1
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
            this.btnConnected = new System.Windows.Forms.Button();
            this.serialPort1 = new System.IO.Ports.SerialPort(this.components);
            this.timer1 = new System.Windows.Forms.Timer(this.components);
            this.btnSendFile = new System.Windows.Forms.Button();
            this.openFileDialog1 = new System.Windows.Forms.OpenFileDialog();
            this.rtbOutput = new System.Windows.Forms.RichTextBox();
            this.btnPing = new System.Windows.Forms.Button();
            this.cmbCOMPort = new System.Windows.Forms.ComboBox();
            this.btnRefreshCOMList = new System.Windows.Forms.Button();
            this.btnClear = new System.Windows.Forms.Button();
            this.btnReset = new System.Windows.Forms.Button();
            this.gbCommButtons = new System.Windows.Forms.GroupBox();
            this.gbCommButtons.SuspendLayout();
            this.SuspendLayout();
            // 
            // btnConnected
            // 
            this.btnConnected.BackColor = System.Drawing.Color.Yellow;
            this.btnConnected.Location = new System.Drawing.Point(137, 15);
            this.btnConnected.Margin = new System.Windows.Forms.Padding(4);
            this.btnConnected.Name = "btnConnected";
            this.btnConnected.Size = new System.Drawing.Size(120, 28);
            this.btnConnected.TabIndex = 0;
            this.btnConnected.Text = "Not Connected";
            this.btnConnected.UseVisualStyleBackColor = false;
            this.btnConnected.Click += new System.EventHandler(this.btnConnected_Click);
            // 
            // serialPort1
            // 
            this.serialPort1.PortName = "COM8";
            // 
            // timer1
            // 
            this.timer1.Tick += new System.EventHandler(this.timer1_Tick);
            // 
            // btnSendFile
            // 
            this.btnSendFile.Location = new System.Drawing.Point(7, 58);
            this.btnSendFile.Margin = new System.Windows.Forms.Padding(4);
            this.btnSendFile.Name = "btnSendFile";
            this.btnSendFile.Size = new System.Drawing.Size(72, 28);
            this.btnSendFile.TabIndex = 8;
            this.btnSendFile.Text = "Send File";
            this.btnSendFile.UseVisualStyleBackColor = true;
            this.btnSendFile.Click += new System.EventHandler(this.btnSendFile_Click);
            // 
            // openFileDialog1
            // 
            this.openFileDialog1.FileName = "openFileDialog1";
            // 
            // rtbOutput
            // 
            this.rtbOutput.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.rtbOutput.Location = new System.Drawing.Point(16, 50);
            this.rtbOutput.Name = "rtbOutput";
            this.rtbOutput.Size = new System.Drawing.Size(316, 423);
            this.rtbOutput.TabIndex = 9;
            this.rtbOutput.Text = "";
            // 
            // btnPing
            // 
            this.btnPing.Location = new System.Drawing.Point(7, 22);
            this.btnPing.Margin = new System.Windows.Forms.Padding(4);
            this.btnPing.Name = "btnPing";
            this.btnPing.Size = new System.Drawing.Size(72, 28);
            this.btnPing.TabIndex = 10;
            this.btnPing.Text = "Ping";
            this.btnPing.UseVisualStyleBackColor = true;
            this.btnPing.Click += new System.EventHandler(this.btnPing_Click);
            // 
            // cmbCOMPort
            // 
            this.cmbCOMPort.FormattingEnabled = true;
            this.cmbCOMPort.Location = new System.Drawing.Point(54, 17);
            this.cmbCOMPort.Name = "cmbCOMPort";
            this.cmbCOMPort.Size = new System.Drawing.Size(76, 24);
            this.cmbCOMPort.TabIndex = 11;
            // 
            // btnRefreshCOMList
            // 
            this.btnRefreshCOMList.Location = new System.Drawing.Point(16, 15);
            this.btnRefreshCOMList.Name = "btnRefreshCOMList";
            this.btnRefreshCOMList.Size = new System.Drawing.Size(38, 28);
            this.btnRefreshCOMList.TabIndex = 12;
            this.btnRefreshCOMList.Text = "...";
            this.btnRefreshCOMList.UseVisualStyleBackColor = true;
            this.btnRefreshCOMList.Click += new System.EventHandler(this.btnRefreshCOMList_Click);
            // 
            // btnClear
            // 
            this.btnClear.Location = new System.Drawing.Point(265, 15);
            this.btnClear.Margin = new System.Windows.Forms.Padding(4);
            this.btnClear.Name = "btnClear";
            this.btnClear.Size = new System.Drawing.Size(52, 28);
            this.btnClear.TabIndex = 13;
            this.btnClear.Text = "Clr";
            this.btnClear.UseVisualStyleBackColor = true;
            this.btnClear.Click += new System.EventHandler(this.btnClear_Click);
            // 
            // btnReset
            // 
            this.btnReset.Location = new System.Drawing.Point(7, 94);
            this.btnReset.Margin = new System.Windows.Forms.Padding(4);
            this.btnReset.Name = "btnReset";
            this.btnReset.Size = new System.Drawing.Size(72, 28);
            this.btnReset.TabIndex = 14;
            this.btnReset.Text = "Reset";
            this.btnReset.UseVisualStyleBackColor = true;
            this.btnReset.Click += new System.EventHandler(this.btnReset_Click);
            // 
            // gbCommButtons
            // 
            this.gbCommButtons.Controls.Add(this.btnPing);
            this.gbCommButtons.Controls.Add(this.btnReset);
            this.gbCommButtons.Controls.Add(this.btnSendFile);
            this.gbCommButtons.Location = new System.Drawing.Point(338, 17);
            this.gbCommButtons.Name = "gbCommButtons";
            this.gbCommButtons.Size = new System.Drawing.Size(88, 456);
            this.gbCommButtons.TabIndex = 15;
            this.gbCommButtons.TabStop = false;
            this.gbCommButtons.Text = "Comm";
            this.gbCommButtons.Visible = false;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.SystemColors.ActiveBorder;
            this.ClientSize = new System.Drawing.Size(431, 485);
            this.Controls.Add(this.gbCommButtons);
            this.Controls.Add(this.btnClear);
            this.Controls.Add(this.btnRefreshCOMList);
            this.Controls.Add(this.cmbCOMPort);
            this.Controls.Add(this.rtbOutput);
            this.Controls.Add(this.btnConnected);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Margin = new System.Windows.Forms.Padding(4);
            this.Name = "Form1";
            this.Text = "C64 Transfer v0.01";
            this.Load += new System.EventHandler(this.Form1_Load);
            this.gbCommButtons.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button btnConnected;
        private System.IO.Ports.SerialPort serialPort1;
        private System.Windows.Forms.Timer timer1;
        private System.Windows.Forms.Button btnSendFile;
        private System.Windows.Forms.OpenFileDialog openFileDialog1;
        private System.Windows.Forms.RichTextBox rtbOutput;
        private System.Windows.Forms.Button btnPing;
        private System.Windows.Forms.ComboBox cmbCOMPort;
        private System.Windows.Forms.Button btnRefreshCOMList;
        private System.Windows.Forms.Button btnClear;
        private System.Windows.Forms.Button btnReset;
        private System.Windows.Forms.GroupBox gbCommButtons;
    }
}

