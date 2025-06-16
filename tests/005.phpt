--TEST--
Check Neptune\Archive\Tar::addFrom
--SKIPIF--
<?php
if (!extension_loaded('neptune')) {
	echo 'skip';
}
?>
--FILE--
<?php
use Neptune\Archive\Tar;

function test_Neptune_Archive_Tar_addFrom() {
	clean();
	$currPath = dirname(realpath(__file__));	
	$cmd = 'cd ' . $currPath . '; tar -cf test.tar 001.phpt 002.phpt';
	exec($cmd, $output, $status);
	if ($status !=0) {
		echo 'skip';
		return;
	}
	$srcTar = new Tar($currPath . '/test.tar', 'r');
	$newTar = new Tar($currPath . '/new.tar', 'w');
	$newTar->addFrom($srcTar, '001.phpt');
	unset($srcTar, $newTar);

	if (!file_exists($currPath . '/out')) {
		if (!mkdir($currPath . '/out', 0755, true)) {
			echo 'skip';
			return;
		}
	}

	$cmd = 'cd ' . $currPath . '; tar -xf new.tar -C out/';
	exec($cmd, $output, $status);
	if ($status !=0) {
		echo 'skip';
		return;
	}

	if (md5_file($currPath . '/001.phpt') == md5_file($currPath . '/out/001.phpt')) {
		echo 'OK';
	}

	clean();
}

function clean() {
	$currPath = dirname(realpath(__file__));
	if (file_exists($currPath . '/test.tar')) {
        unlink($currPath . '/test.tar');
    }
	if (file_exists($currPath . '/new.tar')) {
        unlink($currPath . '/new.tar');
    }

	if (file_exists($currPath . '/out')) {
		$cmd = 'rm -rf ' . $currPath . '/out';
		exec($cmd, $output, $status);
		if ($status !=0) {
			echo 'skip';
			return;
		}
	}
}

test_Neptune_Archive_Tar_addFrom();
?>
--EXPECT--
OK