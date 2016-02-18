<?php require('../vendor/autoload.php');

$host = '127.0.0.1';
$port = '4334';
$db   = 'dictionary';
$password = 'secretpass';

$url = "http://$host:$port/$db";

/*
 * This is just a helper function that makes
 * "Words-Like this" into "words_like_this"
 */
function simplify($word)
{
    $word = strtolower($word);
    $word = str_replace(' ', '_', $word);
    $word = preg_replace('/[^\w-]/', '', $word);
    return $word;
}



/*
 * POST adds items to a specific database
 * note: you must attach X-Auth-Password header
 *
 * @body json array of string -> id
 *
 * @return json object containing status of success
 */
{
    echo "uploading dictionary... ";

    $dictionary_path = '/usr/share/dict/words';
    $words = file($dictionary_path);
    foreach($words as $k => $v) {
        $words[$k] = simplify($v);
    }

    $response = \Httpful\Request::post($url)
        ->addHeader('X-Auth-Password', $password)
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

    $word_to_query = 'teste';
    $response = \Httpful\Request::get("$url?q=" . simplify($word_to_query))
        ->expectsJson()
        ->send();

    foreach($response->body->data as $i) {
        echo $words[$i->id] . "\t=>\t" . $i->d . "\n";
    }
}

/*
 * DELETE deletes all items in a database
 *
 * note: you must attach X-Auth-Password header
 *
 * @return json object containing status of success
 */
{
    echo "deleting...";

    $response = \Httpful\Request::delete("$url")
        ->addHeader('X-Auth-Password', $password)
        ->expectsJson()
        ->send();

    echo (($response->body->status->success) ? 'success!' : 'failed!') . "\n";
}

