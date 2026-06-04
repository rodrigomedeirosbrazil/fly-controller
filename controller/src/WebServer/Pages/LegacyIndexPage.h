#pragma once

const char* LEGACY_INDEX_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>FlyController Manager</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin: 20px; background-color: #f4f4f4; }
        .container { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); max-width: 600px; margin: auto; }
        h1 { color: #333; }
        h2 { color: #555; border-bottom: 1px solid #eee; padding-bottom: 10px; }
        p { color: #666; margin-bottom: 20px; }
        form { margin-top: 20px; margin-bottom: 40px; }
        input[type="file"] { border: 1px solid #ddd; padding: 10px; border-radius: 4px; width: 100%; box-sizing: border-box; margin-bottom: 10px; }
        input[type="submit"] { background-color: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; width: 100%; }
        input[type="submit"]:hover { background-color: #0056b3; }
        .message { margin-top: 20px; padding: 10px; border-radius: 4px; }
        .success { background-color: #d4edda; color: #155724; border-color: #c3e6cb; }
        .error { background-color: #f8d7da; color: #721c24; border-color: #f5c6cb; }

        table { width: 100%; border-collapse: collapse; margin-top: 20px; }
        th, td { text-align: left; padding: 12px; border-bottom: 1px solid #ddd; }
        th { background-color: #f8f9fa; }
        .btn-sm { padding: 5px 10px; font-size: 12px; margin: 2px; text-decoration: none; display: inline-block; border-radius: 3px; color: white; border: none; cursor: pointer; }
        .btn-download { background-color: #28a745; }
        .btn-delete { background-color: #dc3545; }
        .btn-config { background-color: #17a2b8; color: white; padding: 10px 20px; text-decoration: none; display: inline-block; border-radius: 4px; margin: 10px 0; font-size: 16px; }
        .btn-config:hover { background-color: #138496; }
    </style>
</head>
<body>
    <div class="container">
        <h1>FlyController</h1>

        <a href="/config" class="btn-config">Configuration</a>

        <h2>Firmware Update</h2>
        <p>Select a .bin file to update the device.</p>
        <form method="POST" action="/update" enctype="multipart/form-data">
            <input type="file" name="update" accept=".bin">
            <input type="submit" value="Update Firmware">
        </form>
        <div id="response" class="message"></div>

        <h2>Data Logs</h2>
        <p>Download or manage telemetry logs.</p>
        <table id="fileTable">
            <thead>
                <tr>
                    <th>File</th>
                    <th>Size</th>
                    <th>Action</th>
                </tr>
            </thead>
            <tbody>
                <tr><td colspan="3">Loading...</td></tr>
            </tbody>
        </table>
    </div>

    <script>
        document.querySelector('form').addEventListener('submit', function(e) {
            e.preventDefault();
            const form = e.target;
            const formData = new FormData(form);
            const responseDiv = document.getElementById('response');
            responseDiv.className = 'message';
            responseDiv.textContent = 'Updating...';

            fetch('/update', {
                method: 'POST',
                body: formData
            })
            .then(response => response.text())
            .then(data => {
                responseDiv.textContent = data;
                if (data.includes('Success')) {
                    responseDiv.classList.add('success');
                    setTimeout(() => { window.location.reload(); }, 5000);
                } else {
                    responseDiv.classList.add('error');
                }
            })
            .catch(error => {
                responseDiv.textContent = 'Error: ' + error;
                responseDiv.classList.add('error');
            });
        });

        function formatBytes(bytes, decimals = 2) {
            if (bytes === 0) return '0 Bytes';
            const k = 1024;
            const dm = decimals < 0 ? 0 : decimals;
            const sizes = ['Bytes', 'KB', 'MB', 'GB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
        }

        function loadFiles() {
            fetch('/list')
                .then(response => response.json())
                .then(files => {
                    const tbody = document.querySelector('#fileTable tbody');
                    tbody.innerHTML = '';

                    if(files.length === 0) {
                        tbody.innerHTML = '<tr><td colspan="3">No logs found.</td></tr>';
                        return;
                    }

                    files.sort((a, b) => b.name.localeCompare(a.name));

                    files.forEach(file => {
                        const tr = document.createElement('tr');
                        const downloadLink = '/logs' + file.name;

                        tr.innerHTML = `
                            <td>${file.name.replace('/', '')}</td>
                            <td>${formatBytes(file.size)}</td>
                            <td>
                                <a href="${downloadLink}" class="btn-sm btn-download" download>Download</a>
                                <button onclick="deleteFile('${file.name}')" class="btn-sm btn-delete">Delete</button>
                            </td>
                        `;
                        tbody.appendChild(tr);
                    });
                })
                .catch(err => {
                    console.error(err);
                    document.querySelector('#fileTable tbody').innerHTML = '<tr><td colspan="3">Error loading files</td></tr>';
                });
        }

        function deleteFile(filename) {
            if(confirm('Are you sure you want to delete ' + filename + '?')) {
                fetch('/delete?file=' + filename)
                    .then(response => {
                        if(response.ok) {
                            loadFiles();
                        } else {
                            alert('Failed to delete file');
                        }
                    });
            }
        }

        loadFiles();
    </script>
</body>
</html>
)rawliteral";
