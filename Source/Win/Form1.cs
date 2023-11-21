


using System;
using System.IO;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO.Ports;
using static System.Windows.Forms.VisualStyles.VisualStyleElement;

namespace Serial_Logger
{
    public partial class Form1 : Form
    {
        volatile bool USBLost = false;

        //synch with TeensyROM code:
        const UInt16 LaunchFileToken = 0x6444;
        const UInt16 PauseSIDToken   = 0x6466;
        const UInt16 SendFileToken   = 0x64AA;
        const UInt16 AckToken        = 0x64CC;
        const UInt16 FailToken       = 0x9B7F;

        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            btnRefreshCOMList.PerformClick();
            serialPort1.ReadTimeout = 200;
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            string strMsg;

            if (USBLost)
            {
                try
                {
                    serialPort1.Open();
                    USBLost = false;
                    WriteToOutput("\nWe're back!", Color.DarkGreen);
                }
                catch
                {
                    rtbOutput.AppendText(".");
                    rtbOutput.Refresh();
                    return;
                }
            }

            try
            {
                while (serialPort1.BytesToRead != 0)
                {
                    strMsg = ">";
                    try
                    {
                        strMsg += serialPort1.ReadLine();
                    }
                    catch
                    {
                        strMsg += "***";  //timeout indicator
                        while (serialPort1.BytesToRead != 0)
                        {
                            strMsg += (char)serialPort1.ReadChar();
                        }
                        strMsg += "\r";
                        rtbOutput.SelectionColor = Color.Red;
                    }
                    WriteToOutput(strMsg, Color.Blue);
                }
            }
            catch
            {
                WriteToOutput("USB port disconnected, retrying...", Color.DarkRed);
                USBLost = true;
                return;
            }

        }

        private void btnConnected_Click(object sender, EventArgs e)
        {
            if (btnConnected.Text == "Connected")
            {   //disconnect:
                timer1.Enabled = false;
                //if (USBLost == false) 
                try
                {
                    serialPort1.Close();
                }
                catch
                {
                    MessageBox.Show("Unable to close " + cmbCOMPort.Text, "Error");
                }
                btnConnected.Text = "Not Connected";
                btnConnected.BackColor = Color.Yellow;
                pnlCommButtons.Enabled = false;
            }
            else
            {   //connect
                if (cmbCOMPort.Text=="")
                {
                    WriteToOutput("Select COM port", Color.Red);
                    return;
                }
                try
                {
                    serialPort1.PortName = cmbCOMPort.Text;
                    rtbOutput.Clear();
                    serialPort1.Open();
                    //btnPing_Click(null, e);
                }
                catch
                {
                    MessageBox.Show("Unable to open " + cmbCOMPort.Text, "Error");
                    return;
                }
                btnConnected.Text = "Connected";
                btnConnected.BackColor = Color.LightGreen;
                pnlCommButtons.Enabled = true;
                USBLost = false;
                timer1.Enabled = true;
            }
        }

        private void btnPing_Click(object sender, EventArgs e)
        {
            WriteToOutput("Pinging device/C64", Color.Black);
            byte[] Ping = { 0x64, 0x55 };
            serialPort1.Write(Ping, 0, 2); 
        }

        private void btnSendFile_Click(object sender, EventArgs e)
        {
            if(!File.Exists(tbSource.Text))
            {
                WriteToOutput("\nInvalid Source File/Path", Color.DarkRed);
                return;
            }
            
            //Read/store file, get length, calc checksum
            WriteToOutput("\nReading file: " + tbSource.Text, Color.Black);
            BinaryReader br = new BinaryReader(File.Open(tbSource.Text, FileMode.Open));
            UInt32 len = (UInt32)br.BaseStream.Length;
            byte[] fileBuf = br.ReadBytes((Int32)len);  //read full file to array
            br.Close();
            UInt16 CheckSum = 0;
            for (UInt32 num = 0; num < len; num++) CheckSum += fileBuf[num];


            //   App: SendFileToken 0x64AA
            //Teensy: AckToken 0x64CC
            //   App: Send Length(4), CS(2), SD_nUSB(1), 
            //          DestPath/Name(up to MaxNamePathLength, null term)
            //Teensy: AckToken 0x64CC
            //   App: Send file(length)
            //Teensy: AckToken 0x64CC on Pass,  0x9b7f on Fail

            SendIntBytes(SendFileToken, 2);
            if (!GetAck()) return;

            UInt32 SD_nUSB = (rbSDCard.Checked ? 1U:0U);
            if (!tbDestPath.Text.EndsWith("/")) tbDestPath.Text += "/";
            string DestPathFile = tbDestPath.Text;
            DestPathFile += Path.GetFileName(tbSource.Text);

            WriteToOutput("Transferring " + len + " bytes, CS= 0x" + CheckSum.ToString("X4"), Color.Black);
            WriteToOutput("  to TeensyROM " + (SD_nUSB==1U ? "SD:" : "USB:") + DestPathFile, Color.Black);

            SendIntBytes(len, 4);//Send Length
            SendIntBytes(CheckSum, 2);//Send Checksum
            SendIntBytes(SD_nUSB, 1);//Send SD or USB

            serialPort1.Write(DestPathFile + "\0");                    //Send path/name, null terminate
            if (!GetAck()) return;

            WriteToOutput("Sending...", Color.Black);
            Int32 BytesSent = 0;
            while (len>BytesSent)
            {
                Int32 BytesToSend = 16*1024; //block size
                if (len - BytesSent < BytesToSend) BytesToSend = (Int32)len - BytesSent;
                //serialPort1.Write(fileBuf, 0, (Int32)len); //Send file
                serialPort1.Write(fileBuf, BytesSent, BytesToSend); //Send file
                //WriteToOutput("Sent " + BytesToSend, Color.Black);
                rtbOutput.AppendText(".");
                //rtbOutput.ScrollToCaret();
                BytesSent += BytesToSend;
            }
            WriteToOutput("\n...Finished", Color.Black);

            if (!GetAck())
            {
                WriteToOutput("Transfer Failed!", Color.DarkRed);
                return;
            }
            WriteToOutput("Transfer Sucessful!", Color.Green);
            //btnConnected.PerformClick(); //auto disconnect
        }

        bool GetAck(int iTimeoutmSec = 500)
        {
            if (!WaitForSerial(2, iTimeoutmSec)) return false; //sends message on fail

            byte[] recBuf = new byte[2];
            serialPort1.Read(recBuf, 0, 2);
            UInt16 recU16 = to16(recBuf);
            if (recU16 == AckToken)
            {
                WriteToOutput("Ack", Color.DarkGreen);
                return true;
            }
            if (recU16 == FailToken)
            {
                WriteToOutput("Transfer Failed...", Color.DarkRed);
                return false;
            }

            WriteToOutput("Bad Ack: " + recBuf[0].ToString("X2") + ":" + recBuf[1].ToString("X2"), Color.DarkRed);
            return false;
        }

        void SendIntBytes(UInt32 IntToSend, Int16 NumBytes)
        {
            byte[] BytesToSend = BitConverter.GetBytes(IntToSend);
            for(Int16 ByteNum=(Int16)(NumBytes-1); ByteNum>=0; ByteNum--) 
                serialPort1.Write(BytesToSend, ByteNum, 1);
        }

        private void btnRefreshCOMList_Click(object sender, EventArgs e)
        {
            cmbCOMPort.Items.Clear();
            string[] thePortNames = System.IO.Ports.SerialPort.GetPortNames();

            foreach (string item in thePortNames)
            {
                cmbCOMPort.Items.Add(item);
            }

            if (thePortNames.Length != 0)
            {
                cmbCOMPort.SelectedIndex = 0;
            }
            else
            {
                //MessageBox.Show("No COM Ports Found", "Error");
                //cmbCOMPort.Items.Add("COM5");
            }

        }

        private void btnClear_Click(object sender, EventArgs e)
        {
            rtbOutput.Clear();
        }

        private void btnReset_Click(object sender, EventArgs e)
        {
            byte[] Reset = { 0x64, 0xEE };
            serialPort1.Write(Reset, 0, 2);  
        }

        private void btnTest_Click(object sender, EventArgs e)
        {
            byte[] TestCode = { 0x64, 0x67 };
            serialPort1.Write(TestCode, 0, 2);
        }

        private void btnSelectSource_Click(object sender, EventArgs e)
        {
            openFileDialog1.FileName = "";
            openFileDialog1.Filter = "C64 Files (*.prg;*.crt)|*.prg;*.crt|PRG files (*.prg)|*.prg|CRT files (*.crt)|*.crt|All files (*.*)|*.*";
            if (openFileDialog1.ShowDialog() == DialogResult.Cancel) return;
            tbSource.Text = openFileDialog1.FileName;
        }

        private void tbSource_DragDrop(object sender, DragEventArgs e)
        {
            //tbSource.Text = e.ToString();
            string[] files = (string[])e.Data.GetData(DataFormats.FileDrop);
            if (files != null && files.Length != 0)
            {
                tbSource.Text = files[0];
                tbSource.SelectionStart = tbSource.Text.Length;
                tbSource.SelectionLength = 0;
            }
        }

        private void tbSource_DragOver(object sender, DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
                e.Effect = DragDropEffects.Copy;
            else
                e.Effect = DragDropEffects.None;
        }
        private void rbUSBDRive_CheckedChanged(object sender, EventArgs e)
        {
            if (rbUSBDRive.Checked) lblDestPath.Text = "USB Drive Path:";
            else lblDestPath.Text = "SD Card Path:";
        }
        private void btnLaunch_Click(object sender, EventArgs e)
        {
            //   App: LaunchFileToken 0x6444
            //Teensy: AckToken 0x64CC
            //   App: Send CS(2), SD_nUSB(1), 
            //          DestPath/Name(up to MaxNamePathLength, null term)
            //Teensy: AckToken 0x64CC on Pass,  0x9b7f on Fail

            SendIntBytes(LaunchFileToken, 2);
            if (!GetAck()) return;

            UInt32 SD_nUSB = (rbSDCard.Checked ? 1U : 0U);
            //if (!tbDestPath.Text.EndsWith("/")) tbDestPath.Text += "/";
            string DestPathFile = tbDestPath.Text;
            //DestPathFile += Path.GetFileName(tbSource.Text);

            WriteToOutput("Launching " + (SD_nUSB == 1U ? "SD:" : "USB:") + DestPathFile, Color.Black);

            SendIntBytes(SD_nUSB, 1);//Send SD or USB

            serialPort1.Write(DestPathFile + "\0");                    //Send path/name, null terminate
            if (!GetAck()) WriteToOutput("Launch Failed!", Color.Red);
        }

        private void btnPauseSID_Click(object sender, EventArgs e)
        {
            //   App: PauseSIDToken 0x6466
            //Teensy: AckToken 0x64CC on Pass,  0x9b7f on Fail
            SendIntBytes(PauseSIDToken, 2);
            if (GetAck()) WriteToOutput("Sent Pause SID", Color.DarkGreen);
            else WriteToOutput("Pause SID Failed!", Color.Red);
        }

        /********************************  Stand Alone Functions *****************************************/

        private void WriteToOutput(string strMsg, Color color)
        {
            rtbOutput.SelectionColor = color;
            rtbOutput.AppendText(strMsg + "\r");
            rtbOutput.ScrollToCaret();
        }
        private UInt16 to16(byte[] buf)
        {
            return (UInt16)(buf[1]*256 + buf[0]);
        }

        private bool WaitForSerial(int NumBytes, int iTimeoutmSec)
        {
            const int iSleepTimeMs = 10;
            int iCount = 0;

            while (iCount++ <= iTimeoutmSec / iSleepTimeMs)
            {

                if (serialPort1.BytesToRead >= NumBytes) return true;
                //this.Refresh();
                System.Threading.Thread.Sleep(iSleepTimeMs);
            }

            WriteToOutput("Timeout waiting for Teensy", Color.Red);
            //timer1.Enabled = true;
            return false;
        }

    }
}


