<?php
$in = file_get_contents( $argv[1] );
$split = preg_split( '/((?:(?:GET|POST) )|(?:HTTP\/1\.\d \d\d\d ))/', $in, -1, PREG_SPLIT_DELIM_CAPTURE );

array_shift( $split );
//var_dump($split);
for ( $i = 0; $i < count( $split ); $i += 2 ) {
	$n = (int)($i / 2);
	file_put_contents( "{$argv[1]}.$n", $split[$i] . $split[$i+1] );
}


