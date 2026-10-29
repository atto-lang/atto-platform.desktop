import contextlib,os,pathlib,shutil,subprocess,sys,tempfile,unittest
VERIFY=pathlib.Path(__file__).with_name('verify-checkout-raw.py')
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

 def test_skip_worktree_index_flag(self):
  self.git('update-index','--skip-worktree','one.c')
  self.expect_denied()
 def test_assume_unchanged_index_flag(self):
  self.git('update-index','--assume-unchanged','one.c')
  self.expect_denied()
 def test_clean_nested_regular_file(self):
  (self.root/'nested').mkdir();(self.root/'nested'/'ok.c').write_text('int ok;\n')
  self.git('add','nested/ok.c');self.git('commit','-qm','nested');self.head=self.git('rev-parse','HEAD')
  self.assertEqual(self.run_verifier().returncode,0)
 def test_nonregular_fifo_replacement_does_not_block(self):
  if not hasattr(os,'mkfifo'):self.skipTest('POSIX FIFO only')
  (self.root/'one.c').unlink();os.mkfifo(self.root/'one.c')
  self.expect_denied()
 def test_case_colliding_tree_denied(self):
  import importlib.util
  spec=importlib.util.spec_from_file_location('checkout_verifier',VERIFY);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  h=b'0'*40
  raw=b'100644 blob '+h+b'\tReadme\0'+b'100644 blob '+h+b'\tREADME\0'
  with self.assertRaisesRegex(RuntimeError,'case-colliding'):
   m.parse_table(raw,False)
 def test_windows_unsafe_components_denied(self):
  import importlib.util
  spec=importlib.util.spec_from_file_location('checkout_verifier',VERIFY);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  for name in (b'CON.txt',b'foo.',b'sp ace ',b'x:ads',b'folder\\file',b'a/../b',b'COM1',b'NUL.log',b'bad?name'):
   with self.subTest(name=name),self.assertRaises(RuntimeError):m.valid_path(name)
 def test_leaf_swapped_to_symlink_at_open(self):
  if os.name=='nt':self.skipTest('POSIX deterministic O_NOFOLLOW test')
  import importlib.util
  from unittest import mock
  spec=importlib.util.spec_from_file_location('checkout_verifier',VERIFY);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  original=m.open_tracked
  def swap(raw_path,root_fd):
   if raw_path==b'one.c':
    (self.root/'one.c').unlink()
    (self.root/'one.c').symlink_to('script.sh')
   return original(raw_path,root_fd)
  old=os.getcwd()
  try:
   os.chdir(self.root)
   with mock.patch.object(m,'open_tracked',side_effect=swap):
    with self.assertRaises((RuntimeError,OSError)):m.check(self.head)
  finally:os.chdir(old)
 def test_parent_swapped_to_symlink_at_open(self):
  if os.name=='nt':self.skipTest('POSIX deterministic O_DIRECTORY test')
  import importlib.util
  from unittest import mock
  spec=importlib.util.spec_from_file_location('checkout_verifier',VERIFY);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  (self.root/'nested').mkdir();(self.root/'nested'/'ok.c').write_text('int ok;\n')
  self.git('add','nested/ok.c');self.git('commit','-qm','nested');self.head=self.git('rev-parse','HEAD')
  other=self.root.parent/'unsafe-target'
  other.mkdir(exist_ok=True);(other/'ok.c').write_text('int ok;\n')
  original=m.open_tracked
  def swap(raw_path,root_fd):
   if raw_path==b'nested/ok.c':
    (self.root/'nested'/'ok.c').unlink()
    (self.root/'nested').rmdir()
    (self.root/'nested').symlink_to(other,target_is_directory=True)
   return original(raw_path,root_fd)
  old=os.getcwd()
  try:
   os.chdir(self.root)
   with mock.patch.object(m,'open_tracked',side_effect=swap):
    with self.assertRaises((RuntimeError,OSError)):m.check(self.head)
  finally:
   os.chdir(old)
   import shutil
   shutil.rmtree(other,ignore_errors=True)
 def test_after_open_same_bytes_inode_swap_rejected(self):
  if os.name=='nt':self.skipTest('POSIX inode replacement test')
  import importlib.util
  from unittest import mock
  spec=importlib.util.spec_from_file_location('checkout_verifier',VERIFY);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  original=m.open_tracked
  def swap_after_open(raw_path,root_fd):
   fd=original(raw_path,root_fd)
   if raw_path==b'one.c':
    data=(self.root/'one.c').read_bytes()
    (self.root/'one.c').unlink()
    (self.root/'one.c').write_bytes(data)
   return fd
  old=os.getcwd()
  try:
   os.chdir(self.root)
   with mock.patch.object(m,'open_tracked',side_effect=swap_after_open):
    with self.assertRaisesRegex(RuntimeError,'changed between stat and open'):
     m.check(self.head)
  finally:os.chdir(old)


 def test_git_fsmonitor_config_must_not_execute(self):
  if os.name=='nt':self.skipTest('POSIX shell fsmonitor fixture')
  from pathlib import Path
  with tempfile.TemporaryDirectory(prefix='safe_git_monitor_') as tmp:
   marker=Path(tmp)/'EXECUTED'
   script=Path(tmp)/'monitor.sh'
   script.write_text('#!/bin/sh\n: > "'+str(marker)+'"\nprintf "0\\n"\n')
   script.chmod(0o755)
   self.git('config','core.fsmonitor',str(script))
   for args in (('ls-files','--stage','-z'),
                ('ls-files','--others','--exclude-standard','-z'),
                ('ls-files','--others','--ignored','--exclude-standard','-z'),
                ('ls-files','-v','-z')):
    with self.subTest(args=args):
     marker.unlink(missing_ok=True)
     self.git(*args)
     self.assertTrue(marker.exists(),'control: Git ls-files did not trigger fsmonitor')
     marker.unlink()
     result=self.run_verifier()
     self.assertEqual(result.returncode,0,(result.stdout,result.stderr))
     self.assertFalse(marker.exists(),'checkout verifier executed configured Git fsmonitor')
 def test_git_fsmonitor_environment_must_not_execute(self):
  if os.name=='nt':self.skipTest('POSIX shell fsmonitor fixture')
  from pathlib import Path
  from unittest import mock
  with tempfile.TemporaryDirectory(prefix='safe_git_monitor_env_') as tmp:
   marker=Path(tmp)/'EXECUTED'
   script=Path(tmp)/'monitor.sh'
   script.write_text('#!/bin/sh\n: > "'+str(marker)+'"\nprintf "0\\n"\n')
   script.chmod(0o755)
   values={'GIT_CONFIG_COUNT':'1','GIT_CONFIG_KEY_0':'core.fsmonitor',
           'GIT_CONFIG_VALUE_0':str(script)}
   with mock.patch.dict(os.environ,values):
    marker.unlink(missing_ok=True)
    subprocess.run(['git','ls-files','--stage','-z'],cwd=self.root,check=True,
                   stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    self.assertTrue(marker.exists(),'control: injected fsmonitor did not execute')
    marker.unlink()
    result=self.run_verifier()
    self.assertEqual(result.returncode,0,(result.stdout,result.stderr))
    self.assertFalse(marker.exists(),'checkout verifier executed injected Git fsmonitor')

if __name__=='__main__': unittest.main()
