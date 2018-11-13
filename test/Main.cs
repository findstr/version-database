using System;
using System.Net;
using System.IO;
using System.Threading;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using System.Text;
using vdb;

namespace vdb {
class Downloader {
	enum state {
		DOWNLOADING = 0,
		PATCHING = 1,
		FINISH = 2,
	};
	public enum error : int {
		OK,
		NET,		//网络错误
		VER_DOWN,	//版本文件download失败
		FINGER_DOWN,	//指纹文件download失败
		VER_PARSE,	//版本文件格式出错
		PATCH_VERIFY,	//补丁文件校验失败
		FILE_VERIFY,	//数据文件校验失败
		UNEXPECTED,	//不应该出现的错误
		
	};
	private byte[] buf = null;
	private version verobj = null;
	private patcher patch = null;
	private HttpWebRequest HTTP = null;
	private string platform = "android/";
	private string url = "http://192.168.2.118:8000/";
	private string patchurl = null;
	private string fileurl = null;
	public error errno = error.OK;
	public progress progress = new progress();
	public string patchfile = "G:/verx/_patch";
	public string patchdir = "G:/verx/a";

	public class DownError : Exception {
		public DownError(error e) {
			errno = e;
		}
		public error errno;
	};

	[Serializable]
	public class version {
		public string finger = null;
		public string patch = null;
	};
	public void seturl(string url) {
		this.url = url;
	}
	private byte[] download(string uri, string path) {
		try {
			int readn, current = 0, total;
			HTTP = (System.Net.HttpWebRequest)HttpWebRequest.Create(uri);
			HTTP.Timeout = 5000;
			var response = HTTP.GetResponse();
			var ns = response.GetResponseStream();
			total = (int)response.ContentLength;
			@progress.current = 0;
			@progress.total = total;
			buf = new byte[@progress.total];
			while ((readn = ns.Read(buf, current, total - current)) > 0) {
				current += readn;
				@progress.current = current;
			}
			if (path != null)
				File.WriteAllBytes(path, buf);
			return buf;
		} catch (WebException ex) {
			HttpWebResponse wr = (HttpWebResponse)(ex.Response);
			if (wr != null && wr.StatusCode == HttpStatusCode.NotFound) {
				return null;
			}
			throw new DownError(error.NET);
		}
	}
	private version version_parse(byte[] data) {
		var str = Encoding.ASCII.GetString(data);
		try {
			version obj = JsonUtility.FromJson<version>(str);
			return obj;
		} catch (SystemException) {
			throw new DownError(error.VER_PARSE);
		}
	}
	private void verify_and_retry(List<string> fails)
	{
		int retry = 0;
		while (fails != null && retry < 5) {
			@progress.operation = "patch fail full download";
			var iter = fails.GetEnumerator();
			while (iter.MoveNext()) {
				var uri = fileurl + iter.Current;
				var path = Path.Combine(patchdir, iter.Current);
				var dat = download(uri, path);
				if (dat == null) //in theory, it will never happen
					throw new DownError(error.UNEXPECTED);
			}
			fails = patch.tryresume(patchfile);
			++retry;
		}
		if (retry >= 5) //in theory, it will never happen
			throw new DownError(error.UNEXPECTED);
	}
	private void fulldownload() {
		@progress.operation = "full download";
		var uri = fileurl + "/version.json";
		byte[] data = download(uri, null);
		if (data == null)
			throw new DownError(error.VER_DOWN);
		var verobj = version_parse(data);
		patch.setfinger(verobj.finger);
		data = download(fileurl + patcher.FINGERNAME, null);
		if (data == null)
			throw new DownError(error.FINGER_DOWN);
		patch.finger_save(data);
		var fails = patch.verify_all();
		verify_and_retry(fails);
	}
	private bool checklatest() {
		string verhash = patch.finger_hash();
		if (verhash == null) {	//broken? jump to download all files
			fulldownload();
			return true;
		}
		patch.tryresume(patchfile);
		@progress.operation = "check latest";
		patchurl = url + verhash;
		var uri = patchurl + "/version.json";
		byte[] data = download(uri, null);
		if (data == null) {	//borken? jump to download all files
			fulldownload();
			return true;
		}
		verobj = version_parse(data);
		return verhash.CompareTo(verobj.finger) == 0;
	}
	private bool downpatch() {
		@progress.operation = "downloading patch";
		var uri = patchurl + "/patch";
		Debug.Log("Download:" + uri);
		byte[] data = download(uri, patchfile);
		Debug.Log("Download:" + uri + " ok");
		if (!patch.verify_data(data, verobj.patch))
			throw new DownError(error.PATCH_VERIFY);
		Debug.Log("Verify patch Ok");
		return true;
	}
	public void start() {
		try {
			bool islatest;
			this.url = url + platform;
			this.fileurl = this.url + "/latest/";
			@progress.operation = "downloading";
			patch = new patcher(patchdir, @progress);
			islatest = checklatest();
			if (!islatest) {
				downpatch();
				patch.setfinger(verobj.finger);
				var fails = patch.patch(patchfile);
				verify_and_retry(fails);
			}
			@progress.operation = "finish";
		} catch (DownError ex) {
			errno = ex.errno;
			progress.operation = "Error:" + ex.errno;
		}
	}
	public string getprogress(ref int current , ref int total) {
		current = this.progress.current;
		total = this.progress.total;
		return this.progress.operation;
	}
}
}

public class Main : MonoBehaviour {
	const float unit = 1024.0f * 1024.0f;
	public Button ui_download;
	public Text ui_progress;
	public string url = "http://192.168.2.118:8000/";
	public string patchfile = "G:/verx/_patch";
	public string patchdir = "G:/verx/a";
	private Thread T;
	private vdb.Downloader down = null;
	private float progress;
	// Use this for initialization
	private void OnClick() {
		if (down != null) 
			T.Abort();
		Debug.Log("Start Download.");
		down = new vdb.Downloader();
		down.seturl(url);
		down.patchfile = patchfile;
		down.patchdir = patchdir;
		T = new Thread(down.start);
		T.Start();
	}
	void Start () {
		ui_download.onClick.AddListener(OnClick);
	}

	// Update is called once per frame
	void Update () {
		if (down == null)
			return ;
		if (down.errno != Downloader.error.OK) {
			//error
			ui_progress.text = "error:" + down.errno;
		} else {
			string s;
			if (down != null) {
				int total = 0, progress = 0;
				string title = down.getprogress(ref progress, ref total);
				s = String.Format("{0} {1:F}MB/{2:F}MB", title, progress / unit, total / unit);
			} else {
				s = "";
			}
			ui_progress.text = s;
		}
	}
}
