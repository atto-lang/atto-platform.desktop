import contextlib,os,pathlib,shutil,subprocess,sys,tempfile,unittest
VERIFY=pathlib.Path(__file__).with_name('verify_checkout_raw.py')
class Check(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=pathlib.Path(self.tmp.name)
  def git(*args,check=True):return subprocess.run(['git',*args],cwd=self.root,check=check,stdout=subprocess.PIPE,stderr=subprocess.PIPE).stdout.decode().strip()
  self.git=git
  git('init','-q');git('config','user.email','local-test@invalid');git('config','user.name','Local Test')
  (self.root/'one.c').write_bytes(b'int main(void) { return 0; }\n')
  (self.root/'script.sh').write_bytes(b'#!/bin/sh\necho test\n');(self.root/'script.sh').chmod(0o755)
  git('add','.');git('commit','-qm','init');self.head=git('rev-parse','HEAD')
 def run_verifier(self,expected=None):
  return subprocess.run([sys.executable,str(VERIFY)],cwd=self.root,env={**os.environ,'EXPECTED_SHA':expected or self.head},capture_output=True,text=True)
 def expect_denied(self):
  r=self.run_verifier();self.assertEqual(r.returncode,1,(r.stdout,r.stderr))
 def test_clean(self):
  r=self.run_verifier();self.assertEqual(r.returncode,0,(r.stdout,r.stderr));self.assertIn('RAW_CHECKOUT_VERIFIED',r.stdout)
 def test_wrong_sha(self):
  self.assertEqual(self.run_verifier('a'*40).returncode,1)
 def test_missing_sha(self):
  self.assertEqual(self.run_verifier('none').returncode,1)
 def test_dirty_raw(self):
  (self.root/'one.c').write_bytes(b'evil');self.expect_denied()
 def test_staged_index(self):
  (self.root/'one.c').write_text('bad');self.git('add','.');self.expect_denied()
 def test_untracked(self):
  (self.root/'unexpected').write_text('new');self.expect_denied()
 def test_ignored(self):
  (self.root/'.gitignore').write_text('ignored.tmp\n');self.git('add','.gitignore');self.git('commit','-qm','ignore');self.head=self.git('rev-parse','HEAD');(self.root/'ignored.tmp').write_text('new');self.expect_denied()
 def test_hidden_exec_bit(self):
  (self.root/'one.c').chmod(0o755);self.git('config','core.filemode','false');self.expect_denied()
 def test_replaced_symlink(self):
  (self.root/'one.c').unlink();(self.root/'one.c').symlink_to('script.sh');self.expect_denied()
 def test_tracked_symlink(self):
  (self.root/'link').symlink_to('one.c');self.git('add','link');self.git('commit','-qm','link');self.head=self.git('rev-parse','HEAD');self.expect_denied()
 def test_filter_nonexecution(self):
  marker=self.root/'EXECUTED'
  (self.root/'.gitattributes').write_text('one.c filter=evil\n');self.git('config','filter.evil.clean',f'sh -c "touch {marker}; cat"')
  self.git('add','.gitattributes');self.git('commit','-qm','attributes');self.head=self.git('rev-parse','HEAD');marker.unlink(missing_ok=True)
  r=self.run_verifier();self.assertEqual(r.returncode,0,(r.stdout,r.stderr));self.assertFalse(marker.exists())
  self.git('status','--porcelain');self.assertTrue(marker.exists(),'control: Git status should execute configured filter')
 def test_file_content_normalizing_filter(self):
  marker=self.root/'EXECUTED'
  (self.root/'.gitattributes').write_text('one.c filter=evil\n');self.git('config','filter.evil.clean',f'sh -c "touch {marker}; sed s/EVIL/GOOD/g"');(self.root/'one.c').write_bytes(b'GOOD\n');self.git('add','.');self.git('commit','-qm','good');self.head=self.git('rev-parse','HEAD');marker.unlink(missing_ok=True)
  (self.root/'one.c').write_bytes(b'EVIL\n');r=self.run_verifier();self.assertEqual(r.returncode,1,(r.stdout,r.stderr));self.assertFalse(marker.exists())
 def test_not_at_root(self):
  (self.root/'sub').mkdir();r=subprocess.run([sys.executable,str(VERIFY)],cwd=self.root/'sub',env={**os.environ,'EXPECTED_SHA':self.head},capture_output=True,text=True);self.assertEqual(r.returncode,1)
if __name__=='__main__': unittest.main()
