project('weather-station-server', 'c')

json_c_dep = dependency('json-c')
sqlite_dep = dependency('sqlite3')
thread_dep = dependency('threads')

executable('server',
    ['server.c', 'lib/http.c'],
    include_directories: 'lib/',
    dependencies: [json_c_dep, sqlite_dep, thread_dep],
)
