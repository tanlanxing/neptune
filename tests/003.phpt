--TEST--
Check Neptune\Archive\Tar::readFile
--SKIPIF--
<?php
if (!extension_loaded('neptune')) {
	echo 'skip';
}
?>
--FILE--
<?php
use Neptune\Archive\Tar;

function test_Neptune_Archive_Tar_readFile() {
	$currPath = dirname(realpath(__file__));
	if (file_exists($currPath . '/test.tar')) {
        unlink($currPath . '/test.tar');
    }
	$cmd = 'cd ' . $currPath . '; tar -cf test.tar 001.phpt 002.phpt';
	exec($cmd, $output, $status);
	if ($status !=0) {
		echo 'skip';
		return;
	}
	$tar = new Tar($currPath . '/test.tar', 'r');
	$buff = $tar->readFile('001.phpt');
	if (md5($buff) == md5_file($currPath . '/001.phpt')) {
		echo 'OK';
	}
	unset($tar);
	unlink($currPath . '/test.tar');
}

test_Neptune_Archive_Tar_readFile();
?>
--EXPECT--
OK