--TEST--
Check Neptune\Archive\Tar::extractFile
--SKIPIF--
<?php
if (!extension_loaded('neptune')) {
	echo 'skip';
}
?>
--FILE--
<?php
use Neptune\Archive\Tar;

function test_Neptune_Archive_Tar_extractFile() {
	$currPath = dirname(realpath(__file__));
	clean();
	$cmd = 'cd ' . $currPath . '; tar -cf test.tar 001.phpt 002.phpt 2>&1';
	exec($cmd, $output, $status);
	if ($status !=0) {
		print_r($output);
		echo 'skip';
		return;
	}

	if (!file_exists($currPath . '/out')) {
		if (!mkdir($currPath . '/out', 0755, true)) {
			echo 'skip';
			return;
		}
	}

	$target = $currPath . '/out/001.phpt';

	$args = [
		[$currPath . '/test.tar', 'r'],
		[$currPath . '/test.tar', 'r', true]
	];
	foreach($args as $arg) {
		$tar = new Tar(...$arg);
		$tar->extractFile('001.phpt', $target);

		if (!file_exists($target)) {
			echo 'FAILURE: The file has not been extracted yet.';
			clean();
			return;
		}
		if (md5_file($target) != md5_file($currPath . '/001.phpt')) {
			echo 'FAILURE: The target file was diferent from source.';
			clean();
			return;
		}
		$targetStat = stat($target);
		$srcStat = stat($currPath . '/001.phpt');
		foreach(['mode', 'uid', 'gid', 'mtime'] as $key) {
			if ($targetStat[$key] != $srcStat[$key]) {
				echo 'FAILURE: stat.' . $key . ' is different';
				clean();
				return;
			}
		}
		unset($tar);
	}
	clean();
	echo 'OK';
}

function clean() {
	$currPath = dirname(realpath(__file__));
	if (file_exists($currPath . '/test.tar')) {
        unlink($currPath . '/test.tar');
    }

	if (file_exists($currPath . '/out')) {
		$cmd = 'rm -rf ' . $currPath . '/out 2>&1';
		exec($cmd, $output, $status);
		if ($status !=0) {
			print_r($output);
			echo 'skip';
			return;
		}
	}
}

test_Neptune_Archive_Tar_extractFile();
?>
--EXPECT--
OK