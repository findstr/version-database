using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Collections.Generic;
using System.Diagnostics;
using System.Security.Cryptography;
using UnityEngine;

namespace vdb {
	enum action {
		CTRL_NEW	= 0x10,
		CTRL_DFF	= 0x20,
		CTRL_DFX	= 0x30,
		CTRL_MOV	= 0x40,
		CTRL_DEL	= 0x50,
		PATCH_COPY	= 0x01,
		PATCH_INSERT	= 0x02,
	};
	class progress {
		public int current;
		public int total;
		public string operation;
	};
	abstract class CTRL {
		public action act;
		public static action readact(FileStream fs) {
			return (action)fs.ReadByte();
		}
		public static int read8(FileStream fs) {
			return fs.ReadByte();
		}
		public static int read16(FileStream fs) {
			int a = fs.ReadByte();
			int b = fs.ReadByte();
			return b << 8 | a;
		}
		public static int read32(FileStream fs) {
			int a = fs.ReadByte();
			int b = fs.ReadByte();
			int c = fs.ReadByte();
			int d = fs.ReadByte();
			return a | b << 8 | c << 16 | d << 24;
		}

		public byte[] readarr(FileStream fs, int size) {
			byte[] x = new byte[size];
			fs.Read(x, 0, size);
			return x;
		}
		public string readname(FileStream fs) {
			int size = read16(fs);
			byte[] name = readarr(fs, size);
			return System.Text.Encoding.UTF8.GetString(name);
		}
		public byte[] readdata(FileStream fs) {
			int size = read32(fs);
			return readarr(fs, size);
		}
		abstract public void read(FileStream fs);
	};
	class NEW : CTRL {
		public string name;
		public byte[] data;
		public override void read(FileStream fs) {
			act = action.CTRL_NEW;
			name = readname(fs);
			data = readdata(fs);
		}
	};

	class DFF : CTRL {
		public string name;
		public byte[] patch;
		public override void read(FileStream fs) {
			act = action.CTRL_DFF;
			name = readname(fs);
			patch = readdata(fs);
		}
	};

	class DFX : CTRL {
		public string name;
		public string namea;
		public byte[] patch;
		public override void read(FileStream fs) {
			act = action.CTRL_DFX;
			name = readname(fs);
			namea = readname(fs);
			patch = readdata(fs);
		}
	};

	class DEL : CTRL {
		public string name;
		public override void read(FileStream fs) {
			act = action.CTRL_DEL;
			name = readname(fs);
		}
	};

	class MOV : CTRL {
		public string name;
		public string namea;
		public override void read(FileStream fs) {
			act = action.CTRL_MOV;
			name = readname(fs);
			namea = readname(fs);
		}
	};

	class patcher {
		public enum step {
			INVA  = 0,
			BEGIN = 1,
			PATCH = 2,
			DELETE = 3,
			APPLY = 4,
			VERIFY = 5,
			FINISH = 6,
		};
		[Serializable]
		public class infor {
			public int add;
			public int dff;
			public int mov;
			public int del;
			public string hash = null;
			public string fingerhash = null;
			public step step = step.INVA;
		};
		private string patchfile = null;
		private string patchdir_r = null;
		private string patchdir_rw = null;
		private string patchtmp = null;
		private string headname = "_HEAD";
		private SHA1 hashmethod = SHA1.Create();
		private List<DEL> dellist = null;
		private List<string> modified = null;
		//used by verify
		private Dictionary<string, string> fingerprint = null;
		private infor info = null;
		public int total = 100;
		public int current = 0;
		public const string FINGERNAME = "_fingerprint";
		public patcher(string patchdir_r, string patchdir_rw) {
			this.patchdir_r = patchdir_r;
			this.patchdir_rw = patchdir_rw;
			this.patchtmp = Path.Combine(patchdir_rw, "_temp");
		}
		public void finger_save(byte[] dat)
		{
		    var fingerpath = Path.Combine(patchdir_rw, FINGERNAME);
		    writefile(fingerpath, dat);
		}
		public List<string> verify_all()
		{
		    return step_verify();
		}
		private CTRL read(FileStream fs) {
			action act = CTRL.readact(fs);
			CTRL o = null;
			switch (act) {
			case action.CTRL_NEW:
				o = new NEW();
				break;
			case action.CTRL_DFF:
				o = new DFF();
				break;
			case action.CTRL_DFX:
				o = new DFX();
				break;
			case action.CTRL_MOV:
				o = new MOV();
				break;
			case action.CTRL_DEL:
				o = new DEL();
				break;
			}
			o.read(fs);
			return o;
		}
		private byte[] readfile(string name) {
			try {
				return File.ReadAllBytes(name);
			} catch (SystemException) {
				return null;
			}
		}
		private void writefile(string path, byte[] data) {
			FileInfo file = new FileInfo(path);
			file.Directory.Create();
			File.WriteAllBytes(path, data);
		}

		private byte[] readfile_shortname(string name) {
			string path = Path.Combine(patchdir_rw, name);
			var dat = readfile(path);
			if (dat == null) {
				path = Path.Combine(patchdir_r, name);
				dat = readfile(path);
			}
			return dat;
		}
		private int read8(byte[] data, ref int offset) {
			return data[offset++];
		}
		private int read32(byte[] data, ref int offset) {
			int a = data[offset++];
			int b = data[offset++];
			int c = data[offset++];
			int d = data[offset++];
			return a | (b << 8) | (c << 16) | (d << 24);
		}
		private void patch_content(byte[] old, byte[] patch, string topath) {
			int off = 0, end = patch.Length;
			int fsize = read32(patch, ref off);
			FileInfo fi = new FileInfo(topath);
			fi.Directory.Create();
			FileStream fs = new FileStream(topath, FileMode.Create);
			while (off < end) {
				int act = read8(patch, ref off);
				if (act == (int)action.PATCH_COPY) {
					int pos, size;
					pos = read32(patch, ref off);
					size = read32(patch, ref off);
					fs.Write(old, pos, size);
				} else if (act == (int)action.PATCH_INSERT) {
					int size;
					size = read32(patch, ref off);
					fs.Write(patch, off, size);
					off += size;
				}
			}
			fs.Close();
		}
		private void writehead(step s) {
			info.step = s;
			string x = JsonUtility.ToJson(info);
			File.WriteAllText(Path.Combine(patchdir_rw, headname), x);
		}
		private step readhead() {
			try {
				var path = Path.Combine(patchdir_rw, headname);
				if (!File.Exists(path))
					return step.FINISH;
				var head = File.ReadAllText(Path.Combine(patchdir_rw, headname));
				info = JsonUtility.FromJson<infor>(head);
				return info.step;
			} catch (SystemException) {
				return step.INVA;
			}
		}
		private int gettotal() {
			return info.add * 2 + info.dff * 2 + info.mov * 2 + info.del;
		}
		private string format_binary(byte[] data) {
			var sb = new StringBuilder();
			for (int i = 0; i < data.Length; i++)
				sb.AppendFormat("{0:x2}", data[i]);
			return sb.ToString();
		}
		public string finger_hash() {
			var dat = readfile_shortname(FINGERNAME);
			if (dat == null)
				return null;
			var hash = hashmethod.ComputeHash(dat);
			return format_binary(hash);
		}
		public step getstep() {
			if (info != null)
				return info.step;
			else
				return step.BEGIN;
		}
		public string hash_of_file(string name) {
			string hash;
			fingerprint.TryGetValue(name, out hash);
			return hash;
		}
		private bool data_hashcmp(byte[] data, string dsthash) {
			var hash = hashmethod.ComputeHash(data);
			string hashstr = format_binary(hash);
			return hashstr.CompareTo(dsthash) == 0;
		}
		private bool file_hashcmp(string name, string dsthash) {
			var data = readfile_shortname(name);
			if (data == null)
				return false;
			return data_hashcmp(data, dsthash);
		}	
		private void finger_parse() {
			var path = patchdir_rw + "/" + FINGERNAME;
			if (!File.Exists(path))
				path = patchdir_r + "/" + FINGERNAME;
			fingerprint = new Dictionary<string,string>();
			var lines = File.ReadAllLines(path);
			var iter = lines.GetEnumerator();
			while (iter.MoveNext()) {
				string str = (string)iter.Current;
				string[] vals = str.Split(null);
				fingerprint[vals[1]] = vals[0];
			}
			return ;
		}
		private void step_patch() {
			modified = new List<string>();
			FileStream fs = new FileStream(patchfile, FileMode.Open);
			UnityEngine.Debug.Log("step_patch");
			current = 0;
			total = gettotal();
			dellist = new List<DEL>();
			while (fs.Position < fs.Length) {
				CTRL o = read(fs);
				byte[] frombuf = null;
				string topath = null;
				switch (o.act) {
				case action.CTRL_DFF:
					modified.Add(((DFF)o).name);
					topath = Path.Combine(patchtmp, ((DFF)o).name);
					frombuf = readfile_shortname(((DFF)o).name);
					if (frombuf != null)
						patch_content(frombuf, ((DFF)o).patch, topath);
					break;
				case action.CTRL_DFX:
					modified.Add(((DFX)o).name);
					topath = Path.Combine(patchtmp, ((DFX)o).name);
					frombuf = readfile_shortname(((DFX)o).namea);
					if (frombuf != null)
						patch_content(frombuf, ((DFX)o).patch, topath);
					break;
				case action.CTRL_NEW:
					modified.Add(((NEW)o).name);
					topath = Path.Combine(patchtmp, ((NEW)o).name);
					writefile(topath, ((NEW)o).data);
					break;
				case action.CTRL_MOV:
					modified.Add(((MOV)o).name);
					frombuf = readfile_shortname(((MOV)o).namea);
					if (frombuf != null) {
						topath = Path.Combine(patchtmp, ((MOV)o).name);
						writefile(topath, frombuf);
					}
					break;
				case action.CTRL_DEL:
					dellist.Add((DEL)o);
					break;
				}
				if (o.act != action.CTRL_DEL)
					++current;
			}
			fs.Close();
			fs.Dispose();
			writehead(step.PATCH);
		}

		private void step_delete() {
			if (dellist == null) {
				FileStream fs = new FileStream(patchfile, FileMode.Open);
				dellist = new List<DEL>();
				while (fs.Position < fs.Length) {
					CTRL o = read(fs);
					if (o.act == action.CTRL_DEL) {
						dellist.Add((DEL)o);
						break;
					}
				}
				fs.Close();
				fs.Dispose();
			}
			current = info.add + info.dff + info.mov;
			total = gettotal();
			UnityEngine.Debug.Log("step_delet");
			var iter = dellist.GetEnumerator();
			while (iter.MoveNext()) {
				var o = iter.Current;
				try {File.Delete(Path.Combine(patchdir_rw, o.name)); }catch(SystemException) { }
				++current;
			}
			writehead(step.DELETE);
		}
		private void step_apply() {
			UnityEngine.Debug.Log("step_apply");
			current = info.add + info.dff + info.mov + info.del;
			total = gettotal();
			var prefix = patchtmp.Length + 1;
			var files = Directory.GetFiles(patchtmp, "*", SearchOption.AllDirectories).ToList();
			var fiter = files.GetEnumerator();
			while (fiter.MoveNext()) {
				string from = fiter.Current;
				string subname = from.Substring(prefix);
				var to = Path.Combine(patchdir_rw, subname);
				var fi = new FileInfo(to);
				fi.Directory.Create();
				if (File.Exists(to))
					File.Delete(to);
				File.Move(from, to);
				++current;
			}
			writehead(step.APPLY);
		}
		private List<string> step_verify() {
			UnityEngine.Debug.Log("step_verify");
			List<string> fails = new List<string>();
			finger_parse();
			current = 0;
			if (modified != null) {	//full flow, just verify changed
				total = modified.Count;
				var iter = modified.GetEnumerator();
				while (iter.MoveNext()) {
					string hash;
					var subname = iter.Current;
					var path = Path.Combine(this.patchdir_rw, subname);
					fingerprint.TryGetValue(subname, out hash);
					System.Diagnostics.Debug.Assert(hash != null, subname);
					if (hash != null && file_hashcmp(path, hash) == false)
						fails.Add(subname);
					current++;
				}
			} else {
				total = fingerprint.Count;
				var iter = fingerprint.GetEnumerator();
				while (iter.MoveNext()) {
					var subname = iter.Current.Key;
					var hash = iter.Current.Value;
					var dat = readfile_shortname(subname);
					if (data_hashcmp(dat, hash) == false)
						fails.Add(subname);
					current++;
				}
			}
			if (fails.Count > 0)
				return fails;
			writehead(step.VERIFY);
			return null;
		}
		private void step_finish(string patchpath) {
			UnityEngine.Debug.Log("step_finish");
			current = total = gettotal();
			if (Directory.Exists(patchtmp))
				Directory.Delete(patchtmp, true);
			if (File.Exists(patchpath))
				File.Delete(patchpath);
			writehead(step.FINISH);
		}
		public List<string> tryresume(string patchfile, ref bool isbroken) {
			List<string> fails = null;
			this.patchfile = patchfile;
			step s = readhead();
			if (s == step.INVA) {
				isbroken = true;
				return null;
			}
			isbroken = false;
			if (s == step.FINISH)
				return null;
			switch (s) {
			case step.BEGIN:
				goto BEGIN;
			case step.PATCH:
				goto PATCH;
			case step.DELETE:
				goto DELETE;
			case step.APPLY:
				goto APPLY;
			case step.VERIFY:
				goto VERIFY;
			case step.FINISH:
				goto FINISH;
			}
			BEGIN:
				step_patch();
			PATCH:
				step_delete();
			DELETE:
				step_apply();
			APPLY:
				fails = step_verify();
				if (fails != null) 
					return fails;
			VERIFY:
				step_finish(patchfile);
			FINISH:
			return null;
		}
		public List<string> patch(string patchpath, infor info) {
			bool isborken = false;
			this.info = info;
			writehead(step.BEGIN);
			return tryresume(patchpath, ref isborken);
		}

	}
}
