<h1>ping</h1>

ping 명령어는 네트워크 상태를 확인하고 호스트 간의 연결을 테스트하는 데 사용됩니다. 
일반적으로 IP 주소나 도메인 이름을 대상으로 ICMP(Internet Control Message Protocol) 패킷을 보내고, 대상 호스트에서 응답을 받아 응답 시간과 응답의 유무를 확인합니다.

간단하게 말하면, ping은 두 호스트 간의 통신이 가능한지를 확인하는 데 사용됩니다.
ping [대상 주소 또는 도메인] 같은 방식으로 사용하며 아래는 ping google.com을 한 결과입니다.

![스크린샷(779)](https://github.com/ks2019575010/Network_Homework/assets/48661594/f69e78ec-f04e-4e92-a82d-f9c57754f07a)

<h1>tracert</h1>

tracert 명령어는 목적지 호스트까지의 경로를 추적하고, 각 라우터의 IP 주소와 응답 시간을 순차적으로 표시합니다. 
이를 통해 데이터 패킷이 어떻게 전달되는지를 시각적으로 확인할 수 있습니다. 
이 과정에서 응답이 없거나 시간 초과가 발생하는 경우에는 해당 경로에서 문제가 발생한 것일 수 있습니다. 
tracert [대상 주소 또는 도메인] 같은 방식으로 사용하며 아래는 ping google.com을 한 결과입니다.

![스크린샷(780)](https://github.com/ks2019575010/Network_Homework/assets/48661594/056599af-8027-46e5-ad17-f5f6200eabde)


여담으로 traceroute라는 명령어는 비슷한 역할을 하지만 window에선 사용할 수 없습니다.

