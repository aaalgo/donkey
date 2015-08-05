<?php
// Start the session
session_start();
?>
<!DOCTYPE html>
<html>
<body>

<form action="qbic.php" method=POST>
    <table><tr>
     <td>Search URL:</td>
     <td><input name="query_url"></input></td>
     <td>&nbsp;&nbsp;or upload File:</td>
     <td><input type="file" name="query_file" /></td>
     <td><input type="submit" value="Submit"></td>
    </table></tr>
</form>
<br/>

<?php
// Set session variables
$_SESSION["favcolor"] = "green";
$_SESSION["favanimal"] = "cat";
//echo "Session variables are set.";
if (isset($_POST['query_url']) && !empty($_POST['query_url'])) {
    echo "Query by URL:";
    echo $_POST["query_url"]; 
}
else {
    echo "Query by file:";
    switch ($_FILES['query_file']['error']) {
        case UPLOAD_ERR_OK:
            break;
        case UPLOAD_ERR_NO_FILE:
            throw new RuntimeException('No file sent.');
        case UPLOAD_ERR_INI_SIZE:
        case UPLOAD_ERR_FORM_SIZE:
            throw new RuntimeException('Exceeded filesize limit.');
        default:
            throw new RuntimeException('Unknown errors.');
    }
    // You should also check filesize here.
    if ($_FILES['query_file']['size'] > 1000000) {
        throw new RuntimeException('Exceeded filesize limit.');
    }
}
?>
<h2>Uploaded File Info:</h2>
<ul>
<li>Sent file: <?php echo $_FILES['query_file']['name'];  ?>
<li>File size: <?php echo $_FILES['query_file']['size'];  ?> bytes
<li>File type: <?php echo $_FILES['query_file']['type'];  ?>
</ul>
</body>
</html>

