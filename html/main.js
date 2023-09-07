class File_Info {
    file_name;
    file_length;
    file_type;
}
var set_process = function(event) {
    if (event.lengthComputable) {//
        var loaded = event.loaded
        var total = event.total
        var complete = loaded / total;
        var process_div = document.getElementById("process_div");
        process_div.style.display = "block";
        var width = Math.floor(160 * complete);
        console.log(width);
        if(width > 40)
            process_div.style.width = width + "px";
        process_div.innerText = (complete * 100).toFixed(1) + "%";
        }
}

var input_file = function() {
    var input = document.getElementById("upload_input");
    var files = input.files;
    var files_cnt = files.length;
    var file_info = new File_Info;
    var file_read = new FileReader;
    file_read.onload = function() {
        xml_http = new XMLHttpRequest;
        xml_http.upload.onprogress = set_process;
        xml_http.open("POST", "http://192.168.31.59:8800", true);
        xml_http.setRequestHeader("Content-Name" ,file_info.file_name);
        xml_http.setRequestHeader("Content-Type" ,file_info.file_type);
        xml_http.send(this.result);
        console.log(this.result.length);
        console.log("here")

        xml_http.onload = function() {
            if(xml_http.readyState == 4 && xml_http.status == 200) {
                alert("上传成功！！");
                document.getElementById("process_div").style.display = "None";
            }
        }
    }
    for(var i = 0; i < files_cnt; ++i) {
        file_info.file_length = files[i].size;

        console.log(file_info.file_length);
        file_info.file_name = files[i].name;
        file_info.file_type = files[i].type;
        file_read.readAsArrayBuffer(files[i]);
    }
}

var main_func = function() {
    var input = document.getElementById("upload_input");
    input.addEventListener("change", input_file);
}
window.onload = main_func

