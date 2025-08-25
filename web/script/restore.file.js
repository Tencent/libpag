const { exec } = require('child_process');
const path = require('path');

const filePath = path.resolve(__dirname, '../src/pag.ts');

function restoreFile(file) {
    const command = `git restore ${file}`;

    exec(command, (error, stdout, stderr) => {
        if (error) {
            console.error(`执行恢复命令时出错: ${error.message}`);
            return;
        }

        if (stderr) {
            console.error(`stderr: ${stderr}`);
            return;
        }

        console.log(`文件已恢复：${stdout}`);
    });
}

// 调用恢复函数
restoreFile(filePath);
