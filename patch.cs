using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

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
		public byte[] readhash(byte[] data, ref int off) {
			int size = (int)data[off];  off += 1;
			return readarr(data, size, ref off); 
		}
		public byte[] readdata(byte[] data, ref int off) {
			int size = read32(data, ref off);
			return readarr(data, size, ref off);
		}
		abstract public void read(byte[] data, ref int off);
	};
	class NEW : CTRL {
		public byte[] hash;
		public string name;
		public byte[] data;
		public override void read(byte[] x, ref int off) {
			act = action.CTRL_NEW;
			hash = readhash(x, ref off);
			name = readname(x, ref off);
			data = readdata(x, ref off);
		}
	};

	class DFF : CTRL {
		public byte[] hash;
		public string name;
		public byte[] patch;
		public override void read(byte[] data, ref int off) {
			act = action.CTRL_DFF;
			hash = readhash(data, ref off);
			name = readname(data, ref off);
			patch = readdata(data, ref off);
		}
	};

	class DFX : CTRL {
		public byte[] hash;
		public string name;
		public string namea;
		public byte[] patch;
		public override void read(byte[] data, ref int off) {
			act = action.CTRL_DFX;
			hash = readhash(data, ref off);
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
		public byte[] hash;
		public string name;
		public string namea;
		public override void read(byte[] data, ref int off) {
			act = action.CTRL_MOV;
			hash = readhash(data, ref off);
			name = readname(data, ref off);
			namea = readname(data, ref off);
		}
	};

	class patcher {

		public static CTRL read(byte[] data, ref int off) {
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
		public byte[] patch_content(byte[] old, byte[] patch) {
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

		Dictionary<string, byte[]> filecache = new Dictionary<string,byte[]>();
		byte[] readfile(string name) {
			byte[] o;
			bool find = filecache.TryGetValue(name, out o);
			if (find)
				return o;
			o = File.ReadAllBytes(name);
			filecache[name] = o;
			return o;
		}

		void writefile(string path, byte[] data) {
			FileInfo file = new FileInfo(path);
			file.Directory.Create(); 
			File.WriteAllBytes(path, data);
		}

		public void patch(string patchpath, string dir) {
			int offset = 0, end;
			string tmp = Path.Combine(dir, "_temp");
			List<CTRL> second = new List<CTRL>();
			byte[] patch = File.ReadAllBytes(patchpath);
			end = patch.Length;
			//step1 dff/dfx
			while (offset < end) {
				string frompath, topath;
				byte[] frombuf, tobuf;
				CTRL o = read(patch, ref offset);
				switch (o.act) {
				case action.CTRL_DFF:
					frompath = Path.Combine(dir, ((DFF)o).name);
					topath = Path.Combine(tmp, ((DFF)o).name);
					frombuf = readfile(frompath);
					tobuf = patch_content(frombuf, ((DFF)o).patch);
					writefile(topath, tobuf);
					break;
				case action.CTRL_DFX:
					frompath = Path.Combine(dir, ((DFX)o).namea);
					topath = Path.Combine(tmp, ((DFX)o).name);
					frombuf = readfile(frompath);
					tobuf = patch_content(frombuf, ((DFX)o).patch);
					writefile(topath, tobuf);
					break;
				case action.CTRL_NEW:
				case action.CTRL_MOV:
				case action.CTRL_DEL:
					second.Add(o);
					break;
				}
			}
			//step2 rename/delete/new
			var iter = second.GetEnumerator(); 
			while (iter.MoveNext()) {
				byte[] data;
				string temppath, frompath;
				var ctrl = iter.Current;
				switch (ctrl.act) {
				case action.CTRL_NEW:
					temppath = Path.Combine(tmp, ((NEW)ctrl).name);
					writefile(temppath, ((NEW)ctrl).data);
					break;
				case action.CTRL_MOV:
					frompath = Path.Combine(dir, ((MOV)ctrl).namea);
					temppath = Path.Combine(tmp, ((MOV)ctrl).name);
					data = readfile(frompath);
					writefile(temppath, data);
					break;
				case action.CTRL_DEL:
					frompath = Path.Combine(dir, ((DEL)ctrl).name);
					File.Delete(frompath);
					break;
				}
			}
			//step3 merge
			var prefix = tmp.Length + 1;
			var files = Directory.GetFiles(tmp, "*", SearchOption.AllDirectories).ToList();
			var fiter = files.GetEnumerator();
			while (fiter.MoveNext()) {
				string from = fiter.Current;
				string to = from.Substring(prefix);
				to = Path.Combine(dir, to);
				var info = new FileInfo(to);
				info.Directory.Create();
				if (File.Exists(to))
					File.Delete(to);
				File.Move(from, to);
			}
			//step4 verify

			//step5 clearnup
			Directory.Delete(tmp);
		}

	}
}
