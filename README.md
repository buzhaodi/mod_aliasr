# mod_aliasr
freeswich aliasr
### 一个让freeswitch调用阿里的实时语音识别sdk，让识别结果通过esl发送出来的模块
## 如何使用：
  1.把源码拷贝到freeswitch源码的src/mod/applications/mod_aliasr
  
  2.解压tar zxvf linux.tar.gz
  
  3.export export ld_library_path=刚解压出来的路径
  
  4.make install
  
  5.进入cli
  
  6.再dialplan里配置
  ```xml  
  <action application="aliasr_start" data="100"/>
  ```
  如：
    ```  
   <extension name="echo">
   
      <condition field="destination_number" expression="^9199$">
      
        <action application="answer"/>

        <action application="aliasr_start" data="100"/>  
        
<action application="record_session" data="/tmp/record/${strftime(%Y%m%d_%H%M%S)}_${destination_number}.mp4"/>
        <action application="echo"/>
        
      </condition>
      
    </extension>
    
      ```
      
    7.拨打9199 会在fs_cli打印识别结果 订阅CUSTOM  mod_aliasr::asrresult事件，你就能看到识别结果了
