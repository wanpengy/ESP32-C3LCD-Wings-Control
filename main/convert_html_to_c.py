#!/usr/bin/env python3
# -*- coding: utf-8 -*-

def html_to_c_string(html_file, c_file):
    with open(html_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # 转义双引号和反斜杠
    content = content.replace('\\', '\\\\')
    content = content.replace('"', '\\"')
    
    # 按行分割并添加换行符转义
    lines = content.split('\n')
    c_lines = []
    for line in lines:
        c_lines.append('"' + line + '\\n"')
    
    c_string = 'static const char robot_sequence_html[] = \n' + '\n'.join(c_lines) + ';'
    
    with open(c_file, 'w', encoding='utf-8') as out:
        out.write('#include "http_server_ux_enhanced.h"\n')
        out.write('#include "esp_http_server.h"\n')
        out.write('#include "esp_log.h"\n')
        out.write('#include "string.h"\n\n')
        out.write('static const char *TAG = "http_server_files";\n\n')
        out.write(c_string)
        out.write('\n')

if __name__ == '__main__':
    html_to_c_string('robot_sequence_complete.html', 'http_server_files_robot.c')