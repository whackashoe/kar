<?php require('../vendor/autoload.php');

$host = '127.0.0.1';
$port = '4334';
$url = "http://$host:$port";
$dictionary_path = '/usr/share/dict/words';
$word_to_query = 'testy';

function simplify($word)
{
        $word = strtolower($word);
        $word = str_replace(' ', '_', $word);
        $word = preg_replace('/[^\w-]/', '', $word);
        return $word;
}



/*
 * POST performs a replacement of all items in database
 * note: if you want to just add items without cleaing old ones, use PATCH
 *
 * @body json array of string -> id
 *
 * @return json object containing status of success
 */
{
    echo "uploading dictionary... ";
    $words = file($dictionary_path);
    foreach($words as $k => $v) {
        $words[$k] = simplify($v);
    }

    $response = \Httpful\Request::post($url)
        ->body(json_encode(array_flip($words)))
        ->sendsJson()
        ->expectsJson()
        ->send();

    echo (($response->body->status->success) ? 'success!' : 'failed!') . "\n";
}

/*
 * GET performs a search query
 *
 * @param q    search query
 * @param insc insert cost
 * @param delc delete cost
 * @param repc replace cost
 * @param maxc max cost
 *
 * @return json array of object conaining id and distance from search query
 */
{
    echo "querying...\n";

    $response = \Httpful\Request::get("$url?q=" . simplify($word_to_query))
        ->expectsJson()
        ->send();

    foreach($response->body->data as $i) {
        echo $words[$i->id] . "\t=>\t" . $i->d . "\n";
    }
}

