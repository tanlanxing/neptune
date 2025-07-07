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
	$cmd = 'cd ' . $currPath . '; tar -cf test.tar 001.phpt 002.phpt 2>&1';
	exec($cmd, $output, $status);
	if ($status !=0) {
		print_r($output);
		echo 'skip';
		return;
	}
	$args = [
		[$currPath . '/test.tar', 'r'],
		[$currPath . '/test.tar', 'r', true]
	];
	foreach($args as $arg) {
		$tar = new Tar(...$arg);
		$buff = $tar->readFile('001.phpt');
		if (md5($buff) == md5_file($currPath . '/001.phpt')) {
			echo 'OK';
		}
		unset($tar);
	}
	unlink($currPath . '/test.tar');
}

test_Neptune_Archive_Tar_readFile();
?>
--EXPECT--
OKOK