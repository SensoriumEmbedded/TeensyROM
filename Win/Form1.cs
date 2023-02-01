
/*
 * Serial datalogger and RTC setting utility
 * 
 * Written by Travis Smith 2010, trav@tnhsmith.net
 *  
 */

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

namespace Serial_Logger
{
    public partial class Form1 : Form
    {
        volatile bool USBLost = false;

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
                gbCommButtons.Visible = false;
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
                gbCommButtons.Visible = true;
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
            openFileDialog1.FileName = "";
            openFileDialog1.Filter = "C64 Files (*.prg;*.crt)|*.prg;*.crt|PRG files (*.prg)|*.prg|CRT files (*.crt)|*.crt|All files (*.*)|*.*";
            if (openFileDialog1.ShowDialog() == DialogResult.Cancel) return;

            //Read/store file, get length, calc checksum
            WriteToOutput("\nReading file: " + openFileDialog1.SafeFileName, Color.Black);
            BinaryReader br = new BinaryReader(File.Open(openFileDialog1.FileName, FileMode.Open));
            int len = (int)br.BaseStream.Length;
            byte[] fileBuf = br.ReadBytes(len);  //read full file to array
            br.Close();
            UInt16 CheckSum = 0;
            for (UInt32 num = 0; num < len; num++) CheckSum += fileBuf[num];


            //   App: SendFileToken 0x64AA
            //Teensy: ack 0x6464
            //   App: Send Length(2), CS(2), Name(MAX_ROMNAME_CHARS=25 incl term), file(length)
            //Teensy: Pass 0x6480 or Fail 0x9b7f (or anything else received) 
            byte[] recBuf = new byte[2];
            byte[] LenHiLo = { (byte)len, (byte)(len >> 8) };
            byte[] CSHiLo = { (byte)CheckSum, (byte)(CheckSum >> 8) };
            byte[] SendFileToken = { 0x64, 0xAA };
            timer1.Enabled = false;
            WriteToOutput("Transferring " + len + " bytes, CS= " +CheckSum, Color.Black);

            serialPort1.Write(SendFileToken, 0, 2);
            if (!WaitForSerial(2, 500)) return;
            serialPort1.Read(recBuf, 0, 2);
            if (to16(recBuf) != 0x6464)
            {
                WriteToOutput("TeensyROM Not Communicating: " + recBuf[0] + " & " + recBuf[1], Color.DarkRed);
                timer1.Enabled = true;
                return;
            }

            serialPort1.Write(LenHiLo, 0, 2);  //Send Length
            serialPort1.Write(CSHiLo, 0, 2);  //Send Checksum

            //Send name (MAX_ROMNAME_CHARS=25 incl term)
            const int MAX_ROMNAME_CHARS = 25;
            string fname = openFileDialog1.SafeFileName.ToLower();
            if (fname.Length > MAX_ROMNAME_CHARS-1) fname = fname.Substring(0, MAX_ROMNAME_CHARS-5) + fname.Substring(fname.Length - 4); //compress but leave extension

            serialPort1.Write(fname);
            byte[] NullByte = new byte[1] { 0 };
            for (int i = fname.Length; i < MAX_ROMNAME_CHARS; i++) serialPort1.Write(NullByte, 0, 1);

            serialPort1.Write(fileBuf, 0, len); //Send file
            WriteToOutput("Finished sending...", Color.Black);

            if (!WaitForSerial(2, 1000)) return;
            serialPort1.Read(recBuf, 0, 2);
            timer1.Enabled = true;
            if (to16(recBuf) == 0x6480)
            {
                WriteToOutput("Sucess!", Color.Green);
                return;
            }
            else
            {
                WriteToOutput("Failed!" + recBuf[0] + " & " + recBuf[1], Color.DarkRed);
            }
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

        /********************************  Stand Alone Functions *****************************************/

        private void WriteToOutput(string strMsg, Color color)
        {
            rtbOutput.SelectionColor = color;
            rtbOutput.AppendText(strMsg + "\r");
            rtbOutput.ScrollToCaret();
        }
        private UInt16 to16(byte[] buf)
        {
            return (UInt16)(buf[0]*256 + buf[1]);
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
            timer1.Enabled = true;
            return false;
        }


    }
}


