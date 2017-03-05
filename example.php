<?php
/**
 * Copyright 2017 Yuchen Wang <phobosw@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

class Me
{
    public function say($words)
    {
        echo $words."\n";
    }

    public function sleep()
    {
        $this->say('sleeping...');
        sleep(2);
    }

    public function run()
    {
        $this->say('good night');
        $this->sleep();
        $this->say('wake up');
    }
}

$pid = getmypid();
echo "**phptrace sample**\n";
echo "Type these command in a new console:\n";
echo "\n";
echo "trace:                    phptrace -p $pid\n";
echo "trace with filter:        phptrace -p $pid -f type=function,content=sleep\n";
echo "trace with filter:        phptrace -p $pid -f type=class,content=Me\n";
echo "trace with count limit:   phptrace -p $pid -l 2\n";
echo "view status:              phptrace status -p $pid\n";
echo "view status by ptrace:    phptrace status -p $pid --ptrace\n";
echo "\n";

printf("Press enter to continue...\n");
$fp = fopen('php://stdin', 'r');
fgets($fp);
fclose($fp);
usleep(100000);

(new Me)->run();
