using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Collections.Generic;
using System.Diagnostics;
using System.Security.Cryptography;

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
		public static action readact(byte[] data, ref int off) {
			action act = (action)data[off];
			off += 1;
			return act;
		}
		public static int read8(byte[] data, ref int off) {
			int a = data[off];
			off += 1;
			return a;
		}
		public static int read16(byte[] data, ref int off) {
			int a = data[off + 0];
			int b = data[off + 1];
			off += 2;
			return b << 8 | a;
		}
		public static int read32(byte[] data, ref int off) {
			int a = data[off++];
			int b = data[off++];
			int c = data[off++];
			int d = data[off++];
			return a | b << 8 | c << 16 | d << 24;
		}

		public byte[] readarr(byte[] data, int size, ref int off) {
			byte[] x = new byte[size];
			Buffer.BlockCopy(data, off, x, 0, size);
			off += size;
			return x;
		}
		public string readname(byte[] data, ref int off) {
			int size = read16(data, ref off);
			byte[] name = readarr(data, size, ref off);
			return System.Text.Encoding.ASCII.GetString(name);
		}
		public byte[] readdata(byte[] data, ref int off) {
			int size = read32(data, ref off);
			return readarr(data, size, ref off);
		}
		abstract public void read(byte[] data, ref int off);
	};
	class NEW : CTRL {
		public string name;
		public byte[] data;
		public override void read(byte[] x, ref int off) {
			act = action.CTRL_NEW;
			name = readname(x, ref off);
			data = readdata(x, ref off);
		}
	};

	class DFF : CTRL {
		public string name;
		public byte[] patch;
		public override void read(byte[] data, ref int off) {
			act = action.CTRL_DFF;
			name = readname(data, ref off);
			patch = readdata(data, ref off);
		}
	};

	class DFX : CTRL {
		public string name;
		public string namea;
		public byte[] patch;
		public override void read(byte[] data, ref int off) {
			act = action.CTRL_DFX;
			name = readname(data, ref off);
			namea = readname(data, ref off);
			patch = readdata(data, ref off);
		}
	};

	class DEL : CTRL {
		public string name;
		public override void read(byte[] data, ref int off) {
			act = action.CTRL_DEL;
			name = readname(data, ref off);
		}
	};

	class MOV : CTRL {
		public string name;
		public string namea;
		public override void read(byte[] data, ref int off) {
			act = action.CTRL_MOV;
			name = readname(data, ref off);
			namea = readname(data, ref off);
		}
	};

	class patcher {
		enum step {
			BEGIN = 0,
			PATCH = 1,
			DELETE = 2,
			APPLY = 3,
			VERIFY = 4,
			FINISH = 5,
		};
		private progress @progress;
		private string patchdir = null;
		private string patchtmp = null;
		private string headname = "_HEAD";
		private SHA256 hashmethod = SHA256.Create();
		private List<CTRL> patchlist = new List<CTRL>();
		private List<DEL> dellist = new List<DEL>();
		private Dictionary<string, byte[]> filecache = new Dictionary<string,byte[]>();
		//used by verify
		private Dictionary<string, string> fingerprint = new Dictionary<string,string>();
		private string fingerpath = null;
		private string fingerhash = null;
		public const string FINGERNAME = "_fingerprint";
		public patcher(string patchdir, progress obj) {
			this.patchdir = patchdir;
			this.patchtmp = Path.Combine(patchdir, "_temp");
			this.@progress = obj;
			this.fingerpath = Path.Combine(patchdir, FINGERNAME);
		}
		private CTRL read(byte[] data, ref int off) {
			action act = CTRL.readact(data, ref off);
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
			o.read(data, ref off);
			return o;
		}
		private byte[] readfile(string name) {
			byte[] o;
			bool find = filecache.TryGetValue(name, out o);
			if (find)
				return o;
			try {
				o = File.ReadAllBytes(name);
			} catch (SystemException) {
				return null;
			}
			filecache[name] = o;
			return o;
		}
		private void writefile(string path, byte[] data) {
			FileInfo file = new FileInfo(path);
			file.Directory.Create();
			File.WriteAllBytes(path, data);
		}
		private byte[] patch_content(byte[] old, byte[] patch) {
			int wr = 0, off = 0;
			int end = patch.Length;
			int fsize = CTRL.read32(patch, ref off);
			byte[] buff =  new byte[fsize];
			while (off < end) {
				int act = CTRL.read8(patch, ref off);
				if (act == (int)action.PATCH_COPY) {
					int pos, size;
					pos = CTRL.read32(patch, ref off);
					size = CTRL.read32(patch, ref off);
					Buffer.BlockCopy(old, pos, buff, wr, size);
					wr += size;
				} else if (act == (int)action.PATCH_INSERT) {
					int size;
					size = CTRL.read32(patch, ref off);
					Buffer.BlockCopy(patch, off, buff, wr, size);
					off += size;
					wr += size;
				} else {
					return null;
				}
			}
			return buff;
		}
		private void writehead(step s) {
			string[] head = new string[2];
			head[0] = fingerhash;
			head[1] = ((int)s).ToString();
			File.WriteAllLines(Path.Combine(patchdir, headname), head);
		}
		private step readhead() {
			try {
				var head = File.ReadAllLines(Path.Combine(patchdir, headname));
				fingerhash = head[0];
				return (step)Int32.Parse(head[1]);
			} catch (SystemException) {
				fingerhash = finger_hash();
				return step.FINISH;
			}
		}
		private void parse_patch(string patchpath) {
			int offset = 0, end;
			byte[] patch = readfile(patchpath);
			end = patch.Length;
			//step1 dff/dfx
			while (offset < end) {
				CTRL o = read(patch, ref offset);
				switch (o.act) {
				case action.CTRL_DFF:
				case action.CTRL_DFX:
				case action.CTRL_NEW:
				case action.CTRL_MOV:
					patchlist.Add(o);
					break;
				case action.CTRL_DEL:
					dellist.Add((DEL)o);
					break;
				}
			}
		}
		private void step_patch() {
			var iter = patchlist.GetEnumerator();
			while (iter.MoveNext()) {
				byte[] frombuf = null, tobuf = null;
				string frompath = null, topath = null;
				var o = iter.Current;
				switch (o.act) {
				case action.CTRL_DFF:
					frompath = Path.Combine(patchdir, ((DFF)o).name);
					topath = Path.Combine(patchtmp, ((DFF)o).name);
					frombuf = readfile(frompath);
					tobuf = patch_content(frombuf, ((DFF)o).patch);
					writefile(topath, tobuf);
					break;
				case action.CTRL_DFX:
					frompath = Path.Combine(patchdir, ((DFX)o).namea);
					topath = Path.Combine(patchtmp, ((DFX)o).name);
					frombuf = readfile(frompath);
					tobuf = patch_content(frombuf, ((DFX)o).patch);
					writefile(topath, tobuf);
					break;
				case action.CTRL_NEW:
					topath = Path.Combine(patchtmp, ((NEW)o).name);
					writefile(topath, ((NEW)o).data);
					break;
				case action.CTRL_MOV:
					frompath = Path.Combine(patchdir, ((MOV)o).namea);
					topath = Path.Combine(patchtmp, ((MOV)o).name);
					tobuf = readfile(frompath);
					writefile(topath, tobuf);
					break;
				default:
					Debug.Assert(false);
					break;
				}
			}
			writehead(step.PATCH);
		}

		private void step_delete() {
			var iter = dellist.GetEnumerator();
			while (iter.MoveNext()) {
				var o = iter.Current;
				switch (o.act) {
				case action.CTRL_DEL:
					File.Delete(Path.Combine(patchdir, ((DEL)o).name));
					break;
				}
			}
			writehead(step.DELETE);
		}

		private void step_apply() {
			var prefix = patchtmp.Length + 1;
			var files = Directory.GetFiles(patchtmp, "*", SearchOption.AllDirectories).ToList();
			var fiter = files.GetEnumerator();
			while (fiter.MoveNext()) {
				string from = fiter.Current;
				string to = from.Substring(prefix);
				to = Path.Combine(patchdir, to);
				var info = new FileInfo(to);
				info.Directory.Create();
				if (File.Exists(to))
					File.Delete(to);
				File.Move(from, to);
			}
			writehead(step.APPLY);
		}
		private string format_binary(byte[] data) {
			var sb = new StringBuilder();
			for (int i = 0; i < data.Length; i++)
				sb.AppendFormat("{0:x2}", data[i]);
			return sb.ToString();
		}
		public bool verify_data(byte[] data, string dsthash) {
			var hash = hashmethod.ComputeHash(data);
			string hashstr = format_binary(hash);
			return hashstr.CompareTo(dsthash) == 0;
		}
		private bool verify_byname(string name, string dsthash) {
			var path = Path.Combine(patchdir, name);
			var data = readfile(path);
			if (data == null)
				return false;
			return verify_data(data, dsthash);
		}
		public void finger_save(byte[] data) {
			writefile(fingerpath, data);
		}
		public string finger_hash() {
			var dat = readfile(fingerpath);
			if (dat == null)
				return null;
			var hash = hashmethod.ComputeHash(dat);
			return format_binary(hash);
		}
		private void finger_parse() {
			var lines = File.ReadAllLines(fingerpath);
			var iter = lines.GetEnumerator();
			while (iter.MoveNext()) {
				string str = (string)iter.Current;
				string[] vals = str.Split(null);
				fingerprint[vals[1]] = vals[0];
			}
			return ;
		}
		private List<string> step_verify() {
			Debug.Assert(fingerhash != null);
			List<string> fail = new List<string>();
			finger_parse();
			var iter = fingerprint.GetEnumerator();
			while (iter.MoveNext()) {
				var name = iter.Current.Key;
				var hash = iter.Current.Value;
				if (verify_byname(name, hash) != true)
					fail.Add(name);
			}
			filecache.Clear();
			if (fail.Count > 0) {
				return fail;
			}
			writehead(step.VERIFY);
			return null;
		}
		private void step_finish(string patchpath) {
			if (Directory.Exists(patchtmp))
				Directory.Delete(patchtmp, true);
			if (File.Exists(patchpath))
				File.Delete(patchpath);
			writehead(step.FINISH);
		}
		public List<string> verify_all() {
			writehead(step.APPLY);
			return step_verify();
		}
		public void setfinger(string fingerhash) {
			this.fingerhash = fingerhash;
		}
		public List<string> tryresume(string patchfile) {
			step s = readhead();
			if (s == step.FINISH)
				return null;
			List<string> fails;
			fingerpath = Path.Combine(patchdir, FINGERNAME);
			@progress.operation = "patching";
			switch (s) {
			case step.BEGIN:
				parse_patch(patchfile);
				goto BEGIN;
			case step.PATCH:
				parse_patch(patchfile);
				goto PATCH;
			case step.DELETE:
				parse_patch(patchfile);
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

		public List<string> patch(string patchpath) {
			writehead(step.BEGIN);
			return tryresume(patchpath);
		}
	}
}
