#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

// client yapısını oluşturur ve client bilgilerini saklar
typedef struct s_client {
	int id;              // client id si
	char msg[100000];    // clientA gelecek mesajları tutmak için alan
} t_client;

t_client clients[2048]; // 2048 clientlık t_client türünde clients dizisi

// fd_set, select fonksiyonu için kullanılacak veri yapısıdır. hangi soketlerin veri alıp gönderebileceğini takip eder.
fd_set read_set, write_set, current; // okuma, yazma ve mevcut fd kümeleri
int maxfd = 0, gid = 0; // en yüksek fd değeri ve global kimlik sayacı iteratör oluşturulur. maxfd en yüksek fd değerini saklar ve böylece select bu aralığı tarar.
char send_buffer[120000], recv_buffer[120000]; // mesaj gönderebilmek ve alabilmek için kullanılacak geçici bufferlar.

// hata mesajlarını yazmak ve hata durumunda programı exit 1 koduyla sonlandırmak için çağırılır
void err(char *msg)
{
	if (msg)
		write(2, msg, strlen(msg)); // spesifik bir hata mesajı varsa bunu yazdırır.
	else
		write (2, "Fatal error", 11); // spesifik bir mesaj belirtilmediyse default mesajı yazar.
	write(2, "\n", 1); // hata mesajının sonuna newline karakteri basar.
	exit(1); // programı errorla sonlandırır
}

// bir client tarafından gönderilen mesajı gönderici client dışındaki tüm clientlara gönderir.
void send_to_all(int except) // parametre olarak gönderici client'ın id sini alır
{
	for (int fd = 0; fd <= maxfd; fd++) // fd 0 dan başlatılır ve maxfd ye ulaşana kadar döngü devam eder.
		if (FD_ISSET(fd, &write_set) && fd != except) // fd nin write_set yetkisi varsa ("yazılabilir"se) ve gönderici client değilse mesajı gönderir.
			if (send(fd, send_buffer, strlen(send_buffer), 0) == -1) // send_buffer da tutulan mesajı fd ye yani client a gönderir ve hata var mı diye kontrol eder
				err(NULL); // hata durumunda programı sonlandırır
}

int main(int ac, char **av)
{
	if (ac != 2) // port numarası girili mi kontrolü
		err("Wrong number of arguments");

	struct sockaddr_in serveraddr; // sunucu adresi yapısı şablonu oluşturulur
	socklen_t len = sizeof(struct sockaddr_in); // adres boyutu alınır
	int serverfd = socket(AF_INET, SOCK_STREAM, 0); // soket oluşturulur ve fd değişkenine atılır
	if (serverfd == -1)
		err(NULL);

	maxfd = serverfd; // en yüksek fd olarak soket eşitlenir ve sunucu fd si en yüksek fd olmuş olur
	FD_ZERO(&current); // fd kümelerini sıfırlar
	FD_SET(serverfd, &current); // sunucu fd si mevcut kümeye eklenir

	bzero(clients, sizeof(clients)); // clients dizisi sıfırlanır
	bzero(&serveraddr, sizeof(serveraddr)); // sunucu adres yapısı sıfırlanır

	serveraddr.sin_family = AF_INET; // sunucu adres ailesi AF_INET yani IPv4 protokolü olarak verilir
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); // tüm ip adreslerinin dinlenmesi sağlanır
	serveraddr.sin_port = htons(atoi(av[1])); // argüman olarak verilen port numarası dinlenecek port olarak verilir

	if ((bind(serverfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) || listen(serverfd, 100) == -1) // soket bağlantısını yapar ve dinlemeye başlar
		err(NULL);
//--- int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

// sockfd
// Hangi soketi bağlayacağımızı belirten dosya tanıtıcısı. Bu örnekte, serverfd.

// addr
// Soketin bağlanacağı adresi içeren bir yapı. struct sockaddr türündedir.
// Bu nedenle, serveraddr yapısını struct sockaddr * türüne dönüştürmek için cast yapılmıştır:
// (const struct sockaddr *)&serveraddr

// addrlen
// addr parametresinin uzunluğu (bayt cinsinden). Burada sizeof(serveraddr) kullanılır.


//--- 	int listen(int sockfd, int backlog);

// sockfd
// Hangi soketin bağlantı dinlemesi gerektiğini belirtir. Bu örnekte, serverfd.

// backlog
// Gelen bağlantıların bekleme kuyruğu boyutunu belirtir. Örneğin, bu kodda 100 kullanılmış:
// Sunucu aynı anda en fazla 100 bağlantıyı sıraya alabilir.
// Bağlantılar kuyruğu dolarsa, yeni bağlantılar reddedilir.


	while (1) // sonsuz döngü
	{
		read_set = write_set = current; // current fd yi write_set ve read_set e ekler
		if (select(maxfd + 1, &read_set, &write_set, 0, 0) == -1) // etkin fd leri bekler
			continue;

		for (int fd = 0; fd <= maxfd; fd++) // maxfd ye (serverı tutuyor) ulaşana kadar devam et
		{
			if (FD_ISSET(fd, &read_set)) // fd "okunabilir"se gir
			{
				if (fd == serverfd) // fd sunucu soketine bağlanmış yeni bir client ise
				{
					int clientfd = accept(serverfd, (struct sockaddr *)&serveraddr, &len); // yeni bağlantıyı kabul edip clientfd oluşturur
					/*
					Açıklama:
					accept() Fonksiyonu Nedir?
					accept(), bir sunucu soketi üzerinden gelen bir client bağlantısını kabul etmek için kullanılan bir sistem çağrısıdır.
					Bu fonksiyon, sunucu soketine gelen bir bağlantıyı kabul eder ve yeni bir client soketi oluşturur. Yeni client soketi, clientyle iletişim kurmak için kullanılır.
					Parametreler:
					serverfd:

					Bu parametre, sunucu soketinin dosya tanıtıcısıdır. Sunucu, bind() ve listen() fonksiyonlarıyla bu soketi hazırlamış ve dinlemeye başlamıştır.
					accept() fonksiyonu, bu soket üzerinden gelen bir yeni client bağlantısını kabul eder.
					(struct sockaddr *)&serveraddr:

					Bu, client ile sunucu arasında yapılan bağlantının bilgilerini içeren bir yapı (struct sockaddr_in) göstericisi.
					serveraddr aslında bağlantı kabul edilecek olan client bilgisini almak için kullanılan yapıdır. Ancak burada, serveraddr client bilgilerini tutacakken sunucu bağlantısı için kullanılmaz çünkü sunucunun dinlediği soket zaten hazırdır. Burada kullanılmasının nedeni, fonksiyonun gereksinimidir, yani yeni bağlantı üzerinden client adres bilgisini almak için gereklidir.
					Bu parametre, genellikle client bilgisini almak (IP adresi ve port gibi) için kullanılır.
					&len:

					Bu parametre, serveraddr yapısının boyutunu belirten bir socklen_t türündeki değişkendir.
					Başlangıçta, len'in değeri 0 olabilir, ancak accept() çağrısı yapıldığında, bu değer bağlantı kabul edilen clientnin adres bilgileriyle doldurulacaktır. Yani, bağlantı kuran clientnin adres bilgileri bu parametrede yer alacaktır.
					Dönüş Değeri:
					clientfd:
					accept() fonksiyonu başarılı olursa, yeni client soketi dosya tanıtıcısını döndürür. Bu soket üzerinden client ile iletişim kurulabilir.
					Başarısız olursa (örneğin, client bağlantısı kabul edilemezse), -1 döner ve bağlantı hatası oluşmuş olur.
					*/
					if (clientfd == -1)
						continue;
					
					if (clientfd > maxfd)
						maxfd = clientfd;
					clients[clientfd].id = gid++; // yeni clientye id ataması yapılır ve son kalınan id numarası artırılarak bir sonraki atama için güncellenir
					FD_SET(clientfd, &current); // bu fd current setine eklenir

					sprintf(send_buffer, "server: client %d just arrived\n", clients[clientfd].id); // yeni eklenen client ın id si yazılarak mesaj hazırlanır
					send_to_all(clientfd); // mesaj diğer clientlere gönderilerek yeni katılan client bildirilir
				}
				else // yeni bir client bağlantısı yoksa, bir client tarafından mesaj gönderildiyse
				{
					/*
					   buradan devam edilecek!!!!!!!!

					       while (1) {
        read_set = write_set = current; // Mevcut FD kümesini güncelle
        if (select(maxfd + 1, &read_set, &write_set, 0, 0) == -1) continue; // Etkin FD'leri bekle

        for (int fd = 0; fd <= maxfd; fd++) {
            if (FD_ISSET(fd, &read_set)) { // FD okunabilir mi?
                if (fd == serverfd) { // Yeni istemci bağlantısı
                    int clientfd = accept(serverfd, (struct sockaddr *)&serveraddr, &len);
                    if (clientfd == -1) continue;

                    if (clientfd > maxfd) maxfd = clientfd; // maxfd'yi güncelle
                    clients[clientfd].id = gid++; // İstemciye kimlik ata
                    FD_SET(clientfd, &current); // İstemciyi kümeye ekle

                    sprintf(send_buffer, "server: client %d just arrived\n", clients[clientfd].id);
                    send_to_all(clientfd); // Diğer istemcileri bilgilendir
                } else { // İstemciden gelen mesaj
                    int ret = recv(fd, recv_buffer, sizeof(recv_buffer), 0);
                    if (ret <= 0) { // Bağlantı kesildiyse
                        sprintf(send_buffer, "server: client %d just left\n", clients[fd].id);
                        send_to_all(fd);
                        FD_CLR(fd, &current); // FD'yi kümeden çıkar
                        close(fd); // FD'yi kapat
                        bzero(clients[fd].msg, strlen(clients[fd].msg)); // Mesaj tamponunu sıfırla
                    } else { // Gelen mesajı işle
                        for (int i = 0, j = strlen(clients[fd].msg); i < ret; i++, j++) {
                            clients[fd].msg[j] = recv_buffer[i];
                            if (clients[fd].msg[j] == '\n') {
                                clients[fd].msg[j] = '\0';
                                sprintf(send_buffer, "client %d: %s\n", clients[fd].id, clients[fd].msg);
                                send_to_all(fd); // Mesajı diğer istemcilere gönder
                                bzero(clients[fd].msg, strlen(clients[fd].msg)); // Tamponu sıfırla
                                j = -1;
                            }
                        }
                    }
                }
            }
        }
    }
    return (0);
}

					*/
				}
			}
		}
	}
}
